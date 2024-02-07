// SPDX-License-Identifier: BSD-3-Clause

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"
#include "os_graph.h"

/* Create a task that would be executed by a thread. */
os_task_t *create_task(void (*action)(void *), void *arg, void (*destroy_arg)(void *))
{
	os_task_t *t;

	t = malloc(sizeof(*t));
	DIE(t == NULL, "malloc");

	t->action = action;		// the function
	t->argument = arg;		// arguments for the function
	t->destroy_arg = destroy_arg;	// destroy argument function

	return t;
}

/* Destroy task. */
void destroy_task(os_task_t *t)
{
	if (t->destroy_arg != NULL)
		t->destroy_arg(t->argument);
	free(t);
}

/* Put a new task to threadpool task queue. */
void enqueue_task(os_threadpool_t *tp, os_task_t *t)
{
	assert(tp != NULL);
	assert(t != NULL);

	pthread_mutex_lock(&tp->queueMutex);

	list_add_tail(&tp->head, &t->list);

	pthread_mutex_unlock(&tp->queueMutex);
}

/*
 * Check if queue is empty.
 * This function should be called in a synchronized manner.
 */
static int queue_is_empty(os_threadpool_t *tp)
{
	return list_empty(&tp->head);
}

/*
 * Get a task from threadpool task queue.
 * Block if no task is available.
 * Return NULL if work is complete, i.e. no task will become available,
 * i.e. all threads are going to block.
 */

os_task_t *dequeue_task(os_threadpool_t *tp)
{
	os_task_t *t;

	pthread_mutex_lock(&tp->queueMutex);

	if (queue_is_empty(tp)) {
		pthread_mutex_unlock(&tp->queueMutex);
		return NULL;
	}

	// Get the first task from the queue
	t = list_entry(tp->head.next, os_task_t, list);
	list_del(tp->head.next);
	pthread_mutex_unlock(&tp->queueMutex);

	return t;
}

/* Loop function for threads */
static void *thread_loop_function(void *arg)
{
	os_threadpool_t *tp = (os_threadpool_t *) arg;

	pthread_mutex_lock(&tp->mutex);

	// Signal when there are tasks available
	if (tp->taskAvailable == 0)
		pthread_cond_wait(&tp->cond, &tp->mutex);

	pthread_mutex_unlock(&tp->mutex);

	while (1) {
		os_task_t *t;

		t = dequeue_task(tp);
		if (t == NULL)
			break;
		t->action(t->argument);
		destroy_task(t);
	}

	return NULL;
}

/* Wait completion of all threads. This is to be called by the main thread. */
void wait_for_completion(os_threadpool_t *tp)
{
	// Join all worker threads
	for (unsigned int i = 0; i < tp->num_threads; i++)
		pthread_join(tp->threads[i], NULL);
}

/* Create a new threadpool. */
os_threadpool_t *create_threadpool(unsigned int num_threads)
{
	os_threadpool_t *tp = NULL;
	int rc;

	tp = malloc(sizeof(*tp));
	DIE(tp == NULL, "malloc");

	list_init(&tp->head);

	// Initialize mutex for the sum
	rc = pthread_mutex_init(&tp->sumMutex, NULL);
	DIE(rc != 0, "pthread_mutex_init");

	// INitialize mutex for the task queue
	rc = pthread_mutex_init(&tp->queueMutex, NULL);
	DIE(rc != 0, "pthread_mutex_init");

	// Initialize mutex for the state of the nodes
	rc = pthread_mutex_init(&tp->stateMutex, NULL);
	DIE(rc != 0, "pthread_mutex_init");

	pthread_cond_init(&tp->cond, NULL);
	pthread_mutex_init(&tp->mutex, NULL);

	tp->taskAvailable = 0;

	tp->num_threads = num_threads;
	tp->threads = malloc(num_threads * sizeof(*tp->threads));
	DIE(tp->threads == NULL, "malloc");

	for (unsigned int i = 0; i < num_threads; ++i) {
		rc = pthread_create(&tp->threads[i], NULL, &thread_loop_function, (void *) tp);
		DIE(rc < 0, "pthread_create");
	}

	return tp;
}

/* Destroy a threadpool. Assume all threads have been joined. */
void destroy_threadpool(os_threadpool_t *tp)
{
	os_list_node_t *n, *p;
	int rc;

	// Cleanup synchronization mechanisms
	rc = pthread_mutex_destroy(&tp->sumMutex);
	DIE(rc < 0, "pthread_mutex_destroy");

	rc = pthread_mutex_destroy(&tp->queueMutex);
	DIE(rc < 0, "pthread_mutex_destroy");

	rc = pthread_mutex_destroy(&tp->stateMutex);
	DIE(rc < 0, "pthread_mutex_destroy");

	pthread_cond_destroy(&tp->cond);
	pthread_mutex_destroy(&tp->mutex);

	list_for_each_safe(n, p, &tp->head) {
		list_del(n);
		destroy_task(list_entry(n, os_task_t, list));
	}

	free(tp->threads);
	free(tp);
}
