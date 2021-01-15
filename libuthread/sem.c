#include <stddef.h>
#include <stdlib.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "queue.h"
#include "sem.h"
#include "thread.h"

#define SEM_DEBUG false

struct semaphore {
	int value;
	queue_t wait_q;
};

sem_t sem_create(size_t count)
{
	enter_critical_section();
	sem_t new = malloc(sizeof(struct semaphore));
	if (new == NULL) {
		exit_critical_section();
		return NULL;
	} 	
	new->value = count;
	new->wait_q = queue_create();
	if (new->wait_q == NULL){
		exit_critical_section();
		return NULL;
	} 	
	exit_critical_section();
	return new;
}

int sem_destroy(sem_t sem)
{
	if (sem == NULL) return -1;

	enter_critical_section();
	if (queue_length(sem->wait_q) > 0) {
		exit_critical_section();
		return -1;
	}
	if (queue_destroy(sem->wait_q) == -1) {
		exit_critical_section();
		return -1;
	}
	free(sem);
	exit_critical_section();

	return 0;
}

int sem_down(sem_t sem)
{
	if (sem == NULL) return -1;

	enter_critical_section();

	while(sem->value <= 0) {
			// add self to wait queue and block thread
		pthread_t* pthread_id = (pthread_t*)malloc(sizeof(pthread_t));
		*pthread_id = pthread_self();
		queue_enqueue(sem->wait_q,(void*)pthread_id);
		if (SEM_DEBUG) fprintf(stderr,"TID=%ld, sem_down - thread blocking\n",
			pthread_self());
		thread_block();
		if (SEM_DEBUG) fprintf(stderr,"TID=%ld, sem_down - thread waking\n",
			pthread_self());
	}
	sem->value--;
	exit_critical_section();

	return 0;
}

int sem_up(sem_t sem)
{
	pthread_t* pthread_id;

	if (sem == NULL) return -1;

	enter_critical_section();
	sem->value++;
	if (queue_length(sem->wait_q) > 0) {
		// dequeue next thread and wake it up
		assert(queue_dequeue(sem->wait_q,(void**)&pthread_id) == 0);
		if (SEM_DEBUG) fprintf(stderr,"TID=%ld, sem_up - unblocking thread %ld\n",
			pthread_self(),*pthread_id);
		thread_unblock(*pthread_id);
		free((void*)pthread_id);
	} 
	exit_critical_section();

	return 0;
}

int sem_getvalue(sem_t sem, int *sval)
{
	if ((sem == NULL) || (sval == NULL)) return -1;

	enter_critical_section();
	if (sem->value > 0) {
		*sval = sem->value;
	} else {
		*sval = -queue_length(sem->wait_q);
	}
	exit_critical_section();

	return 0;
}

