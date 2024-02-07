// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;

void destroy_arg(void *arg)
{
	os_node_t *node = (os_node_t *)arg;

	free(node->neighbours);
	free(node);
}

void action(void *arg)
{
	if (arg == NULL)
		return;

	os_node_t *node = (os_node_t *)arg;

	// Checking if the node has already been processed
	pthread_mutex_lock(&tp->stateMutex);
	if (graph->visited[node->id] == DONE) {
		pthread_mutex_unlock(&tp->stateMutex);
		return;
	}
	pthread_mutex_unlock(&tp->stateMutex);

	// Adding the sum of the node's info to the global sum
	pthread_mutex_lock(&tp->sumMutex);
	sum += node->info;
	pthread_mutex_unlock(&tp->sumMutex);

	pthread_mutex_lock(&tp->stateMutex);

	// Marking the node as done
	graph->visited[node->id] = DONE;

	pthread_mutex_unlock(&tp->stateMutex);

	// Adding the node's neighbours to the queue
	for (unsigned int i = 0; i < node->num_neighbours; i++) {
		pthread_mutex_lock(&tp->stateMutex);
		if (graph->visited[node->neighbours[i]] == NOT_VISITED) {
			graph->visited[node->neighbours[i]] = PROCESSING;
			enqueue_task(tp, create_task(action, (void *)graph->nodes[node->neighbours[i]], destroy_arg));
		}
		pthread_mutex_unlock(&tp->stateMutex);
	}
}


static void process_node(unsigned int idx)
{
	assert(tp != NULL);
	assert(graph != NULL);

	// Marking the node as processing
	graph->visited[idx] = PROCESSING;

	// Creating task for the node and adding it to the queue
	enqueue_task(tp, create_task(action, (void *)graph->nodes[idx], destroy_arg));

	// Signaling all threads that a task is available
	tp->taskAvailable = 1;
	pthread_cond_broadcast(&tp->cond);
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	// Initialize graph synchronization mechanisms
	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	printf("%d", sum);

	for (unsigned int i = 0; i < graph->num_nodes; i++)
		if (graph->visited[i] == NOT_VISITED) {
			free(graph->nodes[i]->neighbours);
			free(graph->nodes[i]);
		}

	free(graph->nodes);
	free(graph->visited);
	free(graph);
	fclose(input_file);
	return 0;
}
