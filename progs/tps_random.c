#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

#define MAX_NUM_TPS 10
#define MAX_NUM_INTS TPS_SIZE/sizeof(int)

// global array to keep track of tid's
static pthread_t tid[MAX_NUM_TPS];

//global semaphore to make sure that all TPS's get initialize before threads continune
static sem_t sem_init;
static sem_t sem_create_done;
static sem_t sem_clone_done;
static sem_t sem_random_read;


static int buffer[MAX_NUM_INTS];

void* create_clone_tps(void *arg) 
{
	int i = *(int*)arg;
	sem_down(sem_init);

	if (i < MAX_NUM_TPS) sem_down(sem_clone_done);

	sem_up(sem_clone_done);
	
	int clone_tid = rand() % i;
	printf("Thread %d cloning TPS of thread %d\n", i, clone_tid);
	assert(tps_clone(tid[clone_tid]) == 0);
	return NULL;
}

void print_buffer()
{
	printf("Buffer contents:\n");
	for (int i = 0; i < MAX_NUM_INTS; ++i) {
		printf("%d ",buffer[i]);
	}
	printf("\n");
}

void* create_random_tps(void *arg) {
	int i = *(int*)arg;
	int read_buffer[MAX_NUM_INTS];

	if (i < (MAX_NUM_TPS/2)) sem_down(sem_create_done);

	printf("Thread %d creating TPS\n", i);
	tps_create();
	tps_write(0, TPS_SIZE, buffer);
	// print_buffer();

	printf("Thread %d done initializing, waking next thread\n",i);
	sem_up(sem_create_done);

	sem_down(sem_random_read);
	// read a random offset, length
	int offset = rand() % MAX_NUM_INTS;
	int length = rand() % MAX_NUM_INTS;
	if ((offset + length) > MAX_NUM_INTS) {
		length = MAX_NUM_INTS - offset;
	}
	printf("Thread %d reading offset=%d, length=%d\n", i, offset, length);

	tps_read(offset*sizeof(int), length*sizeof(int), read_buffer);
	// for (int j= 0; j < length; ++j) {
	// 	printf("%d ", read_buffer[j]);
	// }
	printf("\n");
	for (int j= 0; j < length; ++j) {
		// printf("%d ", read_buffer[j]);
		if (read_buffer[j] != offset+j) {
			fprintf(stderr,"Thread %d data doesn't match %d vs %d\n",
				i,read_buffer[j],offset+j);
			exit(EXIT_FAILURE);
		} 
	}
	printf("\n");
	sem_up(sem_random_read);
	return NULL;
}

void init_buffer()
{
	for (int i = 0; i < MAX_NUM_INTS; ++i) {
		buffer[i] = i;
	}
	// print_buffer();
}

int main(void)
{
	tps_init(1);

	assert((sem_init = sem_create(0)) != NULL);
	assert((sem_create_done = sem_create(0)) != NULL);
	assert((sem_clone_done = sem_create(0)) != NULL);
	assert((sem_random_read = sem_create(0)) != NULL);

	init_buffer();
	printf("Main thread id = %ld\n",pthread_self());
	for (int i = 0; i < (MAX_NUM_TPS/2); ++i) {
		printf("Creating thread %d\n",i);
		pthread_create(&tid[i], NULL, create_random_tps, &i);
	}	
	// for (int i = (MAX_NUM_TPS/2); i < MAX_NUM_TPS; ++i) {
	// 	pthread_create(&tid[i], NULL, create_clone_tps, &i);
	// }

	for (int i = MAX_NUM_TPS-1; i >= 0; --i) {
		pthread_join(tid[i], NULL);
	}

	sem_destroy(sem_init);
}