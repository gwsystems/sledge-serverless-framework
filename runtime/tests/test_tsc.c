#define _GNU_SOURCE 
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <pthread.h>
#include <threads.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>

#define LISTENER_COUNTS 3
#define WORKER_COUNTS 9
#define LISTENER_THREAD_START_CORE_ID 1


int listener_threads[LISTENER_COUNTS] = {1, 2, 3};
int worker_threads[WORKER_COUNTS] = {4, 5, 6, 7, 8, 9, 10, 11, 12};
int runtime_first_worker_processor = 4;
thread_local int global_worker_thread_idx;
thread_local int dispatcher_id;
thread_local int group_worker_thread_idx;
int diff_cycles[LISTENER_COUNTS][WORKER_COUNTS] = {0};
//thread_local uint64_t current_cycles = 0;
uint64_t current_worker_cycles[WORKER_COUNTS] = {0};
uint64_t current_listener_cycles[LISTENER_COUNTS] = {0};
int runtime_worker_group_size = 3;
int stop = 0;
pthread_t *runtime_worker_threads;
pthread_t *runtime_listener_threads;
thread_local pthread_t listener_thread_id;
thread_local uint8_t dispatcher_thread_idx;
int       *runtime_worker_threads_argument;
int       *runtime_listener_threads_argument;

//#if defined(X86_64) || defined(x86_64)

unsigned long long int
__getcycles(void)
{
        unsigned long long int cpu_time_in_cycles = 0;
        unsigned int           cycles_lo;
        unsigned int           cycles_hi;
        __asm__ volatile("rdtsc" : "=a"(cycles_lo), "=d"(cycles_hi));
        cpu_time_in_cycles = (unsigned long long int)cycles_hi << 32 | cycles_lo;

        return cpu_time_in_cycles;
}

//#endif

void *
worker_thread_main(void *argument)
{
        /* Index was passed via argument */
        global_worker_thread_idx = *(int *)argument;

        /* Set dispatcher id for this worker */
        dispatcher_id = global_worker_thread_idx / runtime_worker_group_size;

        group_worker_thread_idx = global_worker_thread_idx - dispatcher_id * runtime_worker_group_size;

        printf("global thread %d's dispatcher id is %d group size is %d group id is %d\n", global_worker_thread_idx,
            dispatcher_id, runtime_worker_group_size, group_worker_thread_idx);
	while(stop == 0) {
		current_worker_cycles[global_worker_thread_idx] = __getcycles();
	}
}
/**
 * Starts all worker threads and sleeps forever on pthread_join, which should never return
 */
void
runtime_start_runtime_worker_threads()
{
        for (int i = 0; i < WORKER_COUNTS; i++) {
		printf("start %d thread\n", i);
		runtime_worker_threads_argument[i] = i;
                /* Pass the value we want the threads to use when indexing into global arrays of per-thread values */
                int ret = pthread_create(&runtime_worker_threads[i], NULL, worker_thread_main, (void *)&runtime_worker_threads_argument[i]);
                if (ret) {
                        perror("pthread_create");
                        exit(-1);
                }

                cpu_set_t cs;
                CPU_ZERO(&cs);
                CPU_SET(runtime_first_worker_processor + i, &cs);
                ret = pthread_setaffinity_np(runtime_worker_threads[i], sizeof(cs), &cs);
                assert(ret == 0);
        	printf("Starting %d worker thread(s), pin to core %d\n", i, runtime_first_worker_processor + i);
        }
}

void *
listener_thread_main(void *dummy)
{
        /* Index was passed via argument */
    	dispatcher_thread_idx = *(int *)dummy;
	while(stop == 0) {
		current_listener_cycles[dispatcher_thread_idx] = __getcycles();
	}
}

void
listener_thread_initialize(uint8_t thread_id)
{
        printf("Starting listener thread\n");

        cpu_set_t cs;

        CPU_ZERO(&cs);
        CPU_SET(LISTENER_THREAD_START_CORE_ID + thread_id, &cs);

	runtime_listener_threads_argument[thread_id] = thread_id;
	
        int ret = pthread_create(&runtime_listener_threads[thread_id], NULL, listener_thread_main, (void *)&runtime_listener_threads_argument[thread_id]);
        listener_thread_id = runtime_listener_threads[thread_id];
        assert(ret == 0);
        ret = pthread_setaffinity_np(listener_thread_id, sizeof(cpu_set_t), &cs);
        assert(ret == 0);

        printf("\tListener %d thread, pin to core %d\n", thread_id, LISTENER_THREAD_START_CORE_ID + thread_id);
}

void listener_threads_initialize() {
        printf("Starting %d listener thread(s)\n", LISTENER_COUNTS);
        for (int i = 0; i < LISTENER_COUNTS; i++) {
                listener_thread_initialize(i);
        }
}

int main() {
	
	cpu_set_t cs;

        CPU_ZERO(&cs);
        CPU_SET(0, &cs);

	int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cs);
        assert(ret == 0);

	runtime_worker_threads = calloc(WORKER_COUNTS, sizeof(pthread_t));
        assert(runtime_worker_threads != NULL);
	runtime_listener_threads = calloc(LISTENER_COUNTS, sizeof(pthread_t));
        assert(runtime_listener_threads != NULL);
	runtime_worker_threads_argument = calloc(WORKER_COUNTS, sizeof(int));
        assert(runtime_worker_threads_argument != NULL);

	runtime_listener_threads_argument = calloc(WORKER_COUNTS, sizeof(int));
        assert(runtime_listener_threads_argument != NULL);

	runtime_start_runtime_worker_threads();
	listener_threads_initialize();
	
	sleep(5);
	stop = 1;	
	for (int i = 0; i < WORKER_COUNTS; i++) {
                int ret = pthread_join(runtime_worker_threads[i], NULL);
                if (ret) {
                        perror("worker pthread_join");
                        exit(-1);
                }
        }
	
	for (int i = 0; i < LISTENER_COUNTS; i++) {
                int ret = pthread_join(runtime_listener_threads[i], NULL);
                if (ret) {
                        perror("listener pthread_join");
                        exit(-1);
                }
        }

	for(int i = 0; i < LISTENER_COUNTS; i++) {
		for (int j = 0;  j < WORKER_COUNTS; j++) {
			int diff = current_worker_cycles[j] - current_listener_cycles[i];
			diff_cycles[i][j] = diff;
			printf("listener %d diff worker %d is %d\n",  i, j, diff);
		}
	}
}
