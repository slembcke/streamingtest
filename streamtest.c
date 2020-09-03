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

#include "data.h"

u_int64_t GetNanos(void){
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return 1000000000*(u_int64_t)ts.tv_sec + (u_int64_t)ts.tv_nsec;
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

int main(void){
	int blocks = 0;
	char* data = Mapfile("data15", &blocks);
	
	u_int64_t t0 = GetNanos();
	for(unsigned i = 0; i < blocks; i++){
		if(memcmp(data, data + 1*DATA_LENGTH, DATA_LENGTH) != 0){
			fprintf(stderr, "Contents did not match!\n");
			abort();
		}
	}
	uint64_t nanos = GetNanos() - t0;
	
	uint64_t bytes = blocks*DATA_LENGTH;
	printf("read %"PRIu64" MB (%d blocks) %"PRIu64" ms\n", bytes >> 20, blocks, nanos/1000000);
	return EXIT_SUCCESS;
}
