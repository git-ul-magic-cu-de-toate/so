#include "os_threadpool.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* === TASK === */

/* Creates a task that thread must execute */
os_task_t *task_create(void *arg, void (*f)(void *))
{
	os_task_t *new_task = malloc(sizeof(*new_task));

	if (!new_task) {
		printf("[ERROR] Couldn't malloc new task!\n");
		return NULL;
	}

	new_task->argument = arg;
	new_task->task = f;

	return new_task;

}

/* Add a new task to threadpool task queue */
void add_task_in_queue(os_threadpool_t *tp, os_task_t *t)
{
	struct _node *curr_task = tp->tasks;

	struct _node *new_node = malloc(sizeof(*new_node));

	new_node->task = t;
	new_node->next = NULL;

	if (tp->tasks == NULL) {
		tp->tasks = new_node;
		return;
	}

	while (curr_task != NULL) {
		// create new task if last elth in queue
		if (curr_task->next == NULL) {
			curr_task->next = new_node;
			return;
		}

		curr_task = curr_task->next;
	}
}

/* Get the head of task queue from threadpool */
os_task_t *get_task(os_threadpool_t *tp)
{
	if (tp->tasks == NULL)
		return NULL;

	struct _node *curr_task = tp->tasks;

	tp->tasks = curr_task->next;

	os_task_t *returned_task = curr_task->task;

	free(curr_task);

	return returned_task;
}

/* === THREAD POOL === */

/* Initialize the new threadpool */
os_threadpool_t *threadpool_create(unsigned int nTasks, unsigned int nThreads)
{
	os_threadpool_t *new_tp = malloc(sizeof(*new_tp));

	pthread_mutex_init(&new_tp->taskLock, NULL);

	new_tp->threads = malloc(nThreads * sizeof(pthread_t));

	new_tp->num_threads = nThreads;
	new_tp->should_stop = 0;
	new_tp->tasks = NULL;

	for (int i = 0; i < nThreads; i++)
		pthread_create(&new_tp->threads[i], NULL, thread_loop_function, new_tp);

	return new_tp;
}

/* Loop function for threads */
void *thread_loop_function(void *args)
{
	os_threadpool_t *tp = (os_threadpool_t *) args;

	while (1) {
		// check if threadpool should stop
		pthread_mutex_lock(&tp->taskLock);
		if (tp->should_stop == 1) {
			pthread_mutex_unlock(&tp->taskLock);
			return NULL;
		}

		// check if any tasks are free
		os_task_t *runtime = get_task(tp);

		if (runtime == NULL) {
			pthread_mutex_unlock(&tp->taskLock);
			continue;
		}
		pthread_mutex_unlock(&tp->taskLock);

		runtime->task(runtime->argument);
	}

	return NULL;
}

/* Stop the thread pool once a condition is met */
void threadpool_stop(os_threadpool_t *tp, int (*processingIsDone)(os_threadpool_t *))
{
	while (!processingIsDone(tp))
		tp->should_stop = 0;

	tp->should_stop = 1;

	for (int i = 0; i < tp->num_threads; i++)
		pthread_join(tp->threads[i], NULL);

	free(tp->tasks);
	free(tp->threads);
	pthread_mutex_destroy(&tp->taskLock);
	free(tp);
}
