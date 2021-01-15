#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <stdbool.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

#define TPS_DEBUG false

// Data structure to hold global information on TPS blocks
struct tps_global_struct {
	// for now, use queue to hold list of tps's - should upgrade this to 
	// a hash table (instead of doing linear search through queues)
	//
	// Another option: use thread specific storage (in pthread lib)?
	queue_t tps_list;
};
struct tps_global_struct* tps_global = NULL;

struct tps_page {
	int ref_count;
	void* addr;
};
typedef struct tps_page* tps_page_t;

struct tps_block {
	pthread_t tid;
	tps_page_t page;
};
typedef struct tps_block* tps_block_t;


int match_tid(void* data, void *arg) {
	pthread_t tid = *(pthread_t*)arg;
	tps_block_t tps = (tps_block_t)data;
	if (tps->tid == tid) return 1;
	return 0;
}

tps_block_t tps_find_block(pthread_t tid)
{
	void* addr = NULL;
	pthread_t local_tid = tid;
	queue_iterate(tps_global->tps_list, match_tid, (void*)&local_tid, &addr);
	return addr;
}

void* tps_get_block_addr() 
{
	tps_block_t tps = tps_find_block(pthread_self());
	assert(tps != NULL);
	return tps->page->addr;
}

int match_page(void* data, void *arg) {
	int* addr = *(int**)arg;
	tps_block_t tps = (tps_block_t)data;
	if (TPS_DEBUG) fprintf(stderr, "Comparing 0x%lx to 0x%lx\n",
		(long unsigned int)tps->page->addr, (long unsigned int)addr);
	if (tps->page->addr == addr) return 1;
	return 0;
}

tps_block_t tps_find_page(void* addr)
{
	void* tps = NULL;
	void* local_addr = addr;
	if (TPS_DEBUG) fprintf(stderr, "tps_find_page() @0x%lx\n",
		(long unsigned int)addr);
	queue_iterate(tps_global->tps_list, match_page, (void*)&local_addr, &tps);
	return tps;
}

static void segv_handler(int sig, siginfo_t *si, __attribute__((unused)) void *context)
{
    /*
     * Get the address corresponding to the beginning of the page where the
     * fault occurred
     */
    void *p_fault = (void*)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));

	if (TPS_DEBUG) fprintf(stderr, "segv_handler @0x%lx, p_fault=0x%lx\n",
		(long unsigned int)si->si_addr,(long unsigned int)p_fault);
    /*
     * Iterate through all the TPS areas and find if p_fault matches one of them
     */
    if (tps_find_page(p_fault) != NULL)
        /* Printf the following error message */
        fprintf(stderr, "TPS protection error!\n");

    /* In any case, restore the default signal handlers */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    /* And transmit the signal again in order to cause the program to crash */
    raise(sig);
}

int tps_init(int segv)
{
	if (tps_global != NULL) return -1;

	tps_global = (struct tps_global_struct*)malloc(sizeof(struct tps_global_struct));
	if ((tps_global->tps_list = queue_create()) == NULL) return -1;

	if (segv) {
		struct sigaction sa;

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = segv_handler;
		sigaction(SIGBUS, &sa, NULL);
		sigaction(SIGSEGV, &sa, NULL);
	}

	return 0;
}

tps_page_t tps_create_page()
{
	// allocate a tps_page struct
	tps_page_t tps_page = (tps_page_t)malloc(sizeof(struct tps_page));
	assert(tps_page != NULL);

	// mmap a block of memory
	// tps_page->addr = mmap(NULL, TPS_SIZE, PROT_READ | PROT_WRITE,
	// 	MAP_ANONYMOUS | MAP_PRIVATE,-1,0);	
	tps_page->addr = mmap(NULL, TPS_SIZE, PROT_NONE,
		MAP_ANONYMOUS | MAP_PRIVATE,-1,0);
	if (tps_page->addr == NULL) {
		perror("failed to mmap tps");
		exit(EXIT_FAILURE);
	}
	tps_page->ref_count = 1;
	return tps_page;
}

tps_page_t tps_clone_tps_page(tps_page_t page_to_clone)
{
	tps_page_t page = tps_create_page();
	page_to_clone->ref_count--;

	if (TPS_DEBUG) fprintf(stderr, "TID=%ld, cloning page 0x%lx from 0x%lx\n",
		pthread_self(), 
		(long unsigned int)page->addr, 
		(long unsigned int)page_to_clone->addr);
	if (mprotect(page->addr, TPS_SIZE, PROT_WRITE) != 0) {
		perror("Error on mprotect");
		exit(EXIT_FAILURE);
	}	
	if (mprotect(page_to_clone->addr, TPS_SIZE, PROT_READ) != 0) {
		perror("Error on mprotect");
		exit(EXIT_FAILURE);
	}	
	memcpy(page->addr,page_to_clone->addr,TPS_SIZE);
	if (mprotect(page->addr, TPS_SIZE, PROT_NONE) != 0) {
		perror("Error on mprotect");
		exit(EXIT_FAILURE);
	}	
	if (mprotect(page_to_clone->addr, TPS_SIZE, PROT_NONE) != 0) {
		perror("Error on mprotect");
		exit(EXIT_FAILURE);
	}	
	return page;
}

int tps_create(void)
{
	enter_critical_section();
	if (TPS_DEBUG) fprintf(stderr, "TID=%ld, tps_create()\n",
		pthread_self());

	// check to see if tps already exists
	tps_block_t tps = tps_find_block(pthread_self());
	if (tps != NULL) {
		exit_critical_section();
		return -1;
	}

	// allocate a tps_block struct
	tps = (tps_block_t)malloc(sizeof(struct tps_block));
	assert(tps != NULL);	
	tps->page = tps_create_page();
	tps->tid = pthread_self();
	queue_enqueue(tps_global->tps_list,tps);
	exit_critical_section();
	if (TPS_DEBUG) fprintf(stderr, "TID=%ld, created TPS block @0x%lx\n",
		pthread_self(), (long unsigned int)tps->page->addr);
	return 0;
}

int tps_destroy(void)
{
	enter_critical_section();
	tps_block_t tps = tps_find_block(pthread_self());
	if (tps == NULL) return -1;
	// check that referenc count is 1...
	if (tps->page->ref_count == 1) {
		int unmap_result = munmap(tps->page->addr, TPS_SIZE);
		if (unmap_result != 0) {
			perror("Could not unmap TPS");
			exit(EXIT_FAILURE);
		}
		free(tps->page);
	} else {
		tps->page->ref_count--;
	}
	assert(queue_delete(tps_global->tps_list, tps) == 0);
	free(tps);
	exit_critical_section();
	return 0;
}


void tps_print_buffer(char* buffer, size_t length)
{
	if (TPS_DEBUG) fprintf(stderr, "Buffer Contents - length = %ld:\n", length);
	for (int i = 0; i < length; ++i) {
		fprintf(stderr, "%c", *(buffer+i));
	}
	fprintf(stderr, "\n");
}

int tps_read(size_t offset, size_t length, void *buffer)
{
	// should check bounds of memory read to see if within block
	enter_critical_section();
	void* block = tps_get_block_addr();
	if (mprotect(block, TPS_SIZE, PROT_READ) != 0) {
		perror("Error on mprotect");
		exit(EXIT_FAILURE);
	}	
	memcpy(buffer,block+offset,length);
	// if (TPS_DEBUG) tps_print_buffer(block+offset,length);
	if (mprotect(block, TPS_SIZE, PROT_NONE) != 0) {
		perror("Error on mprotect");
		exit(EXIT_FAILURE);
	}	
	exit_critical_section();
	return 0;
}

int tps_write(size_t offset, size_t length, void *buffer)
{
	// should check bounds of memory write to see if within block
	enter_critical_section();
	tps_block_t tps = tps_find_block(pthread_self());
	if (tps == NULL) {
		exit_critical_section();
		return -1;
	}

	// check ref_count to see if entire page needs to be copied
	if (tps->page->ref_count > 1) {
		tps->page = tps_clone_tps_page(tps->page);
	}
	void* block = tps_get_block_addr();
	if (TPS_DEBUG) fprintf(stderr, "TID=%ld, writing %ld bytes to 0x%lx from 0x%lx\n",
		pthread_self(), length, (long unsigned int)block+offset, 
		(long unsigned int)buffer);

	if (mprotect(block, TPS_SIZE, PROT_WRITE) != 0) {
		perror("Error on mprotect");
		exit(EXIT_FAILURE);
	}
	memcpy(block+offset,buffer,length);
	// if (TPS_DEBUG) tps_print_buffer(block+offset,length);
	if (mprotect(block, TPS_SIZE, PROT_NONE) != 0) {
		perror("Error on mprotect");
		exit(EXIT_FAILURE);
	}	
	exit_critical_section();
	return 0;
}

int tps_clone(pthread_t tid)
{
	enter_critical_section();
	if (TPS_DEBUG) fprintf(stderr, "TID=%ld, tps_clone(%ld)\n",
		pthread_self(),tid);	
	// check to see if thread already has TPS
	if (tps_find_block(pthread_self()) != NULL) {
		exit_critical_section();
		return -1;
	}
	tps_block_t tps_to_clone = tps_find_block(tid);
	// allocate a tps_block struct
	tps_block_t tps = (tps_block_t)malloc(sizeof(struct tps_block));
	assert(tps != NULL);
	tps->page = tps_to_clone->page;
	tps->page->ref_count++;
	tps->tid = pthread_self();
	queue_enqueue(tps_global->tps_list,tps);
	exit_critical_section();
	return 0;
}

