#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "os_list.h"

#define MAX_TASK 100
#define MAX_THREAD 4

int sum;
os_graph_t *graph;
os_threadpool_t *tp;
pthread_mutex_t graph_lock;

void processNode(unsigned int nodeIdx)
{
	os_node_t *node = graph->nodes[nodeIdx];

	// adds node value to the sum
	pthread_mutex_lock(&graph_lock);
	sum += node->nodeInfo;
	pthread_mutex_unlock(&graph_lock);

	// iterates through neighbors
	for (int i = 0; i < node->cNeighbours; i++) {
		pthread_mutex_lock(&graph_lock);

		// check if neighbor wasnt visited, if not proccess it
		if (graph->visited[node->neighbours[i]] == 0) {
			graph->visited[node->neighbours[i]] = 1;
			pthread_mutex_unlock(&graph_lock);

			processNode(node->neighbours[i]);
		} else {
			// unlock the thread for checking next neighbor
			pthread_mutex_unlock(&graph_lock);
		}
	}
}

void traverse_task(void *arg)
{
	int node_idx = *(int *)arg;

	pthread_mutex_lock(&graph_lock);
	if (graph->visited[node_idx] == 0) {
		graph->visited[node_idx] = 1;
		pthread_mutex_unlock(&graph_lock);

		processNode(node_idx);
	} else {
		pthread_mutex_unlock(&graph_lock);
	}
}

int processed_done(os_threadpool_t *tp)
{
	if (tp->tasks == NULL)
		return 1;

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: ./parallel input_file\n");
		exit(1);
	}

	FILE *input_file = fopen(argv[1], "r");

	if (input_file == NULL) {
		printf("[Error] Can't open file\n");
		return -1;
	}

	graph = create_graph_from_file(input_file);
	if (graph == NULL) {
		printf("[Error] Can't read the graph from file\n");
		return -1;
	}

	// created threadpool
	tp = threadpool_create(MAX_TASK, MAX_THREAD);

	pthread_mutex_init(&graph_lock, NULL);

	int *args = malloc(graph->nCount * sizeof(*args));

	for (int i = 0; i < graph->nCount; i++) {
		args[i] = i;

		// adds new task in queue
		pthread_mutex_lock(&tp->taskLock);
		add_task_in_queue(tp, task_create(&args[i], traverse_task));
		pthread_mutex_unlock(&tp->taskLock);
	}

	threadpool_stop(tp, processed_done);

	printf("%d", sum);
	return 0;
}
