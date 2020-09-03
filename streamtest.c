#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "tinycthread.h"

#define TINA_IMPLEMENTATION
// #define _TINA_ASSERT(_COND_, _MESSAGE_) //{ if(!(_COND_)){fprintf(stdout, _MESSAGE_"\n"); abort();} }
#include "tina.h"

#define TINA_JOBS_IMPLEMENTATION
#include "tina_jobs.h"

#include "data.h"

u_int64_t GetNanos(void){
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return 1000000000*(u_int64_t)ts.tv_sec + (u_int64_t)ts.tv_nsec;
}

typedef struct {
	thrd_t thread;
	tina_scheduler* sched;
	unsigned queue_idx;
	unsigned thread_id;
} worker_context;

#define MAX_WORKERS 256
static unsigned WORKER_COUNT;
worker_context WORKERS[MAX_WORKERS];

static int WorkerBody(void* data){
	worker_context* ctx = data;
	tina_scheduler_run(ctx->sched, ctx->queue_idx, false, ctx->thread_id);
	return 0;
}

void StartThreads(tina_scheduler* sched){
	WORKER_COUNT = sysconf(_SC_NPROCESSORS_ONLN);
	printf("Starting %d worker threads.\n", WORKER_COUNT);
	
	for(unsigned i = 0; i < WORKER_COUNT; i++){
		worker_context* worker = WORKERS + i;
		(*worker) = (worker_context){.sched = sched, .queue_idx = 0, .thread_id = i};
		thrd_create(&worker->thread, WorkerBody, worker);
	}
}

static void* Mapfile(const char* name, int *blocks){
	int fd = open(name, 0);
	struct stat stats;
	fstat(fd, &stats);
	
	*blocks = stats.st_size/DATA_LENGTH;
	assert(stats.st_size % DATA_LENGTH == 0);
	
	void* ptr = mmap(NULL, stats.st_size, PROT_READ, MAP_SHARED, fd, 0);
	assert(ptr);
	return ptr;
}

typedef struct {
	void* data;
	int blocks;
	
	int block;
} BlockContext;

static void BlockJob(tina_job* job, void* user_data, unsigned* thread_id){
	BlockContext* ctx = user_data;
	
	unsigned idx = (61*ctx->block) & (ctx->blocks - 1);
	if(memcmp(ctx->data, ctx->data + idx*DATA_LENGTH, DATA_LENGTH) != 0){
		fprintf(stderr, "Contents did not match!\n");
		abort();
	}
}

int main(void){
	int blocks = 0;
	void* data = Mapfile("data15", &blocks);
	
	tina_scheduler* sched = tina_scheduler_new(2*blocks, 1, 32, 64*1024);
	StartThreads(sched);
	
	tina_group group;
	tina_group_init(&group);
	
	BlockContext contexts[blocks];
	tina_job_description descs[blocks];
	for(unsigned i = 0; i < blocks; i++){
		contexts[i] = (BlockContext){.data = data, .blocks = blocks, .block = i};
		descs[i] = (tina_job_description){.func = BlockJob, .user_data = contexts + i};
	}
	tina_scheduler_enqueue_batch(sched, descs, blocks, &group);
	// tina_scheduler_enqueue(sched, NULL, BlockJob, NULL, 0, &group);
	
	u_int64_t t0 = GetNanos();
	tina_scheduler_wait_blocking(sched, &group, 0);
	uint64_t nanos = GetNanos() - t0;
	
	uint64_t bytes = blocks*DATA_LENGTH;
	printf("read %"PRIu64" MB (%d blocks) in %"PRIu64" ms\n", bytes >> 20, blocks, nanos/1000000);
	printf("%.2f GB/s\n", 1e9*bytes/nanos/1024/1024/1024);
	
	return EXIT_SUCCESS;
}
