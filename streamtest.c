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
#include "lz4.h"
#include "lz4frame.h"

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

static tina_scheduler* SCHED;
static unsigned WORKER_COUNT;
static void* DATA;
static unsigned BLOCK_COUNT;

static int WorkerBody(void* data){
	worker_context* ctx = data;
	tina_scheduler_run(ctx->sched, ctx->queue_idx, false, ctx->thread_id);
	return 0;
}

static void BlockJob(tina_job* job, void* user_data, unsigned* thread_id){
	// if(memcmp(DATA, user_data, DATA_LENGTH) != 0){
	// 	fprintf(stderr, "Contents did not match!\n");
	// 	abort();
	// }
	
	void* buffer = malloc(BLOCK_SIZE);
	
	LZ4F_dctx* dctx;
	LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
	size_t dst_size = BLOCK_SIZE;
	size_t src_size = DATA_LENGTH;
	size_t result = LZ4F_decompress(dctx, buffer, &dst_size, user_data, &src_size, NULL);
	assert(result == 0);
	assert(dst_size == BLOCK_SIZE);
	assert(src_size == DATA_LENGTH);
	
	LZ4F_freeDecompressionContext(dctx);
	free(buffer);
}

static void RunJobs(tina_job* job, void* user_data, unsigned* thread_id){
	tina_job_description* descs = user_data;
	
	tina_group group;
	tina_group_init(&group);
	
	unsigned cursor = 0;
	while(cursor < BLOCK_COUNT){
		// for(unsigned i = 0; i < WORKER_COUNT*2; i++){
		// 	void* ptr = descs[cursor + i].user_data;
		// 	madvise(ptr, DATA_LENGTH, MADV_SEQUENTIAL);
		// }
		
		cursor += tina_scheduler_enqueue_throttled(SCHED, descs + cursor, BLOCK_COUNT - cursor, &group, WORKER_COUNT*2);
		tina_job_wait(job, &group, WORKER_COUNT);
	}
	tina_job_wait(job, &group, 0);
}

static uint64_t RunSequentialSingle(){
	u_int64_t t0 = GetNanos();
	// madvise(DATA, BLOCK_COUNT*DATA_LENGTH, MADV_SEQUENTIAL);
	for(unsigned i = 0; i < BLOCK_COUNT; i++){
		if(memcmp(DATA, DATA + i*DATA_LENGTH, DATA_LENGTH) != 0){
			fprintf(stderr, "Contents did not match!\n");
			abort();
		}
	}
	return GetNanos() - t0;
}

static uint64_t RunRandomParallel(){
	// Start job system.
	SCHED = tina_scheduler_new(1024, 1, 32, 64*1024);
	
	WORKER_COUNT = sysconf(_SC_NPROCESSORS_ONLN);
	worker_context WORKERS[WORKER_COUNT];
	
	printf("Starting %d worker threads.\n", WORKER_COUNT);
	for(unsigned i = 0; i < WORKER_COUNT; i++){
		worker_context* worker = WORKERS + i;
		(*worker) = (worker_context){.sched = SCHED, .queue_idx = 0, .thread_id = i};
		thrd_create(&worker->thread, WorkerBody, worker);
	}
	
	// Setup jobs.
	tina_job_description descs[BLOCK_COUNT];
	for(unsigned i = 0; i < BLOCK_COUNT; i++){
		unsigned idx = (61*i) & (BLOCK_COUNT - 1);
		descs[i] = (tina_job_description){.func = BlockJob, .user_data = DATA + idx*DATA_LENGTH};
	}
	
	tina_group group;
	tina_group_init(&group);
	tina_scheduler_enqueue(SCHED, NULL, RunJobs, descs, 0, &group);
	
	// Wait for jobs to finish.
	u_int64_t t0 = GetNanos();
	tina_scheduler_wait_blocking(SCHED, &group, 0);
	return GetNanos() - t0;
}

int main(void){
	// Map data.
	int fd = open("data15", 0);
	struct stat stats;
	fstat(fd, &stats);
	
	BLOCK_COUNT = stats.st_size/DATA_LENGTH;
	assert(stats.st_size % DATA_LENGTH == 0);
	
	DATA = mmap(NULL, stats.st_size, PROT_READ, MAP_SHARED, fd, 0);
	assert(DATA);
	
	// uint64_t nanos = RunSequentialSingle();
	uint64_t nanos = RunRandomParallel();
	printf("read %"PRIu64" MB (%d blocks) in %"PRIu64" ms\n", stats.st_size >> 20, BLOCK_COUNT, nanos/1000000);
	printf("%.2f GB/s raw\n", 1e9*stats.st_size/nanos/1024/1024/1024);
	printf("%.2f GB/s lz4\n", 1e9*((size_t)BLOCK_SIZE*(size_t)BLOCK_COUNT)/nanos/1024/1024/1024);
	
	return EXIT_SUCCESS;
}
