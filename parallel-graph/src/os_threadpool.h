/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __OS_THREADPOOL_H__
#define __OS_THREADPOOL_H__	1

#include <pthread.h>
#include <semaphore.h>
#include "os_list.h"

typedef struct {
	void *argument;
	void (*action)(void *arg);
	void (*destroy_arg)(void *arg);
	os_list_node_t list;
} os_task_t;

typedef struct os_threadpool {
	unsigned int num_threads;
	pthread_t *threads;

	/*
	 * Head of queue used to store tasks.
	 * First item is head.next, if head.next != head (i.e. if queue
	 * is not empty).
	 * Last item is head.prev, if head.prev != head (i.e. if queue
	 * is not empty).
	 */
	os_list_node_t head;

	// This mutex is used to avoid race condition when adding the info to the sum
	pthread_mutex_t sumMutex;

	// This mutex is used to avoid race condition when adding or removing tasks from the queue
	pthread_mutex_t queueMutex;

	// This mutex is used to avoid race condition when reading or writing the state of a node
	pthread_mutex_t stateMutex;

	// This variable is used to see if there are any tasks available
	int taskAvailable;

	// This condition variable is used to signal the threads that there are tasks available
	pthread_cond_t cond;

	// This mutex is used to block the threads when there are no tasks available
	pthread_mutex_t mutex;
} os_threadpool_t;

os_task_t *create_task(void (*f)(void *), void *arg, void (*destroy_arg)(void *));
void destroy_task(os_task_t *t);

os_threadpool_t *create_threadpool(unsigned int num_threads);
void destroy_threadpool(os_threadpool_t *tp);

void enqueue_task(os_threadpool_t *q, os_task_t *t);
os_task_t *dequeue_task(os_threadpool_t *tp);
void wait_for_completion(os_threadpool_t *tp);

#endif
