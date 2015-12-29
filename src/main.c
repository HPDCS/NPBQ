/*****************************************************************************
*
*	This file is part of NBQueue, a lock-free O(1) priority queue.
*
*   Copyright (C) 2015, Romolo Marotta
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
******************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <pthread.h>
#include <stdarg.h>
#include <sys/time.h>

#include "datatypes/nonblocking_queue.h"
#include "datatypes/list.h"
#include "datatypes/calqueue.h"

#include "mm/myallocator.h"


nonblocking_queue* nbqueue;
list(bucket_node) lqueue;

int payload = 0;
struct timeval startTV;
volatile double GVT = 0.0;

char		DATASTRUCT;
unsigned int TOTAL_OPS;		// = 800000;
unsigned int THREADS;		// Number of threads
unsigned int OPERATIONS; 	// Number of operations per thread
unsigned int PRUNE_PERIOD;	// Number of ops before calling prune
double PROB_ROLL;			// Control parameter for increasing the probability to enqueue // a node with timestamp lower than the current owned by the thread
double PROB_DEQUEUE;		// Probability to dequeue
double MEAN_INTERARRIVAL_TIME;			// Maximum distance from the current event owned by the thread
char   PROB_DISTRIBUTION;
unsigned int LOG_PERIOD;	// Number of ops before printing a log
unsigned int INIT_SIZE;		// Define the starting size of the queue
unsigned int VERBOSE;		// if 1 prints a full log on STDOUT and on individual files
unsigned int LOG;			// = 0;
double PRUNE_TRESHOLD;		// = 0.35;
double BUCKET_WIDTH;		// = 1.0;//0.000976563;
unsigned int COLLABORATIVE_TODO_LIST;
unsigned int SAFETY_CHECK;
unsigned int EMPTY_QUEUE;

unsigned int *id;
volatile long long *ops;
volatile long long *ops_count;
struct timeval *malloc_time;
struct timeval *free_time;
unsigned int *malloc_count;
unsigned int *free_count;
volatile double* volatile array;
FILE **log_files;

void test_log(unsigned int my_id, const char *msg, ...) {
	char buffer[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buffer, 1024, msg, args);
	va_end(args);

	printf("%s",buffer);
	fwrite(buffer,1,  strlen(buffer), log_files[my_id]);
}



double uniform_rand(struct drand48_data *seed, struct random_data *state)
{
	unsigned int random_int;
	double random_num = 0.0;
	random_r(state, &random_int);
	random_num = ((double)random_int) / RAND_MAX;
	//drand48_r(seed, &random_num);
	random_num *= MEAN_INTERARRIVAL_TIME*2;
	return random_num;
}

double triangular_rand(struct drand48_data *seed, struct random_data *state)
{
	double random_num = 0.0;
	unsigned int random_int;
	random_r(state, &random_int);
	random_num = ((double)random_int) / RAND_MAX;
	//drand48_r(seed, &random_num);
	random_num = sqrt(random_num);
	random_num *= MEAN_INTERARRIVAL_TIME*3/2;
	return random_num;
}

double exponential_rand(struct drand48_data *seed, struct random_data *state)
{
	double random_num = 0.0;
	unsigned int random_int = 0;
	while(D_EQUAL(random_num, 0.0))
	{
		random_r(state, &random_int);
		random_num = ((double)random_int) / RAND_MAX;
		//drand48_r(seed, &random_num);
	}
	random_num =  -log(random_num);
	random_num *= MEAN_INTERARRIVAL_TIME;
	return random_num;
}

void* process(void *arg)
{
	struct timeval endTV, diff;
	char name_file[128];
	unsigned int my_id;
	unsigned int random_int;
	long long n_dequeue = 0;
	long long n_enqueue = 0;
	struct drand48_data seed;
	struct random_data state;
	double random_num = 0.0;

	double local_min = 0.0;
	long long tot_count = 0;
	FILE  *f;




	my_id =  *((unsigned int*)(arg));
	lid = my_id;
	sprintf(name_file, "%u.txt", my_id);
	//srand48_r(my_id, &seed);
	srandom_r(my_id, &state);

	if(VERBOSE)
	{
		f = fopen(name_file, "w+");
		log_files[my_id] = f;
		test_log(my_id,  "HI! I'M ALIVE %u\n", my_id);
	}
	array[my_id] = 0.0;

	while(tot_count < TOTAL_OPS)
	{
		//gettimeofday(&endTV, NULL);
		//timersub(&endTV, &startTV, &diff);


		//drand48_r(&seed, &random_num);
		//random_r(&state, &random_int);
		random_num = ((double)random_int) / RAND_MAX;

		if( random_num < (PROB_DEQUEUE))
		{
			double timestamp = INFTY;
			unsigned int counter = 1;
			void* free_pointer;

			if(DATASTRUCT == 'N')
			{
				bucket_node *new = dequeue(nbqueue);
				free_pointer = new;
				timestamp = new->timestamp;
				counter = new->counter;
			}
			else if(DATASTRUCT == 'L')
			{
				free_pointer = list_pop(lqueue);
				bucket_node *new = node_payload(lqueue,free_pointer);
				if(free_pointer != NULL)
				{
					timestamp = new->timestamp;
					counter = new->counter;
				}
			}
			else if(DATASTRUCT == 'C')
			{
				calqueue_node* new = calqueue_get();
				free_pointer = new;
				if(free_pointer != NULL)
				{
					timestamp = new->timestamp;
					counter = 1;
				}
			}
			if(timestamp == INFTY)
			{
				if( VERBOSE )
					test_log(my_id, "%u-%d:%d\tDEQUEUE EMPTY\n", my_id, diff.tv_sec, diff.tv_usec);
			}
			else
			{
				n_dequeue++;

				if( VERBOSE )
					test_log(my_id, "%u-%d:%d\tDEQUEUE %.15f - %d\n", my_id, diff.tv_sec, diff.tv_usec, timestamp, counter);

				array[my_id] = timestamp;

				if ( SAFETY_CHECK )
				{
					if(timestamp < GVT)
					{
						printf("%u - %d:%d ERRORE timestamp:%f > GVT:%f\n",
								my_id, (int)diff.tv_sec, (int)diff.tv_usec, timestamp, GVT);
						exit(1);
					}
					unsigned int j =0;
					for(;j<THREADS;j++)
					{
						double tmp;
						do
							tmp = array[j];
						while( tmp < timestamp || ( D_EQUAL(timestamp, tmp) && j < my_id) );
					}
					GVT = timestamp;
				}


				local_min = timestamp;
			}
			if(counter == 0)
			{
				printf("%u-%d:%d\tDEQUEUE should never return a HEAD node %.10f - %d\n",
						my_id, (int)diff.tv_sec, (int)diff.tv_usec, timestamp, counter);
				exit(1);
			}

			if(free_pointer != NULL)
				mm_free(free_pointer);
		}
		else
		{
			double timestamp = 0.0;
			int counter = 0;
			double update = 0.0;

			if(PROB_DISTRIBUTION == 'U')
				update = uniform_rand(&seed, &state);
			else if(PROB_DISTRIBUTION == 'T')
				update = triangular_rand(&seed, &state);
			else if(PROB_DISTRIBUTION == 'E')
				update = exponential_rand(&seed, &state);

			timestamp = local_min;

			timestamp += update;
			if(timestamp < 0.0)
				timestamp = 0;

			if(DATASTRUCT == 'N')
				counter =	enqueue(nbqueue, timestamp, NULL);
			else if(DATASTRUCT == 'L')
			{
				bucket_node node;
				node.timestamp = timestamp;
				node.counter = 1;
				list_insert(lqueue, timestamp, &node);
			}
			else if(DATASTRUCT == 'C')
				calqueue_put(timestamp, NULL);

			//local_min = timestamp;

			if( VERBOSE )
				test_log(my_id, "%u-%d:%d\tENQUEUE %.15f - %u\n", my_id, (int)diff.tv_sec, (int)diff.tv_usec, timestamp, counter);

			n_enqueue++;

		}

		if( DATASTRUCT == 'N' && ops_count[my_id]%(PRUNE_PERIOD) == 0)
		{
			double min = INFTY;
			unsigned int j =0;
			for(;j<THREADS;j++)
			{
				double tmp = array[j];
				if(tmp < min)
					min = tmp;
			}
			prune(nbqueue, min*PRUNE_TRESHOLD);

			if( VERBOSE )
				test_log(my_id, "%u-%d:%d\tPRUNE %.10f\n", my_id, (int)diff.tv_sec, (int)diff.tv_usec, min*PRUNE_TRESHOLD);

		}

		if(LOG && my_id == 0 && ops_count[my_id]%(LOG_PERIOD) == 0)
		{
			double min = INFTY;
			unsigned int j =0;
			for(;j<THREADS;j++)
			{
				double tmp = array[j];
				if(tmp < min)
					min = tmp;
			}

			gettimeofday(&endTV, NULL);
			timersub(&endTV, &startTV, &diff);
			printf("%u - LOG %.10f %.2f/100.00 SEC:%d:%d\n", lid, min, ((double)ops_count[my_id])*100/OPERATIONS, (int)diff.tv_sec, (int)diff.tv_usec);
		}

		ops_count[my_id]++;
		unsigned int j=0;
		tot_count = 0;
		for(j=0;j<THREADS;j++)
			tot_count += ops_count[j];
	}

	if(LOG && my_id == 0 && ops_count[my_id]%(LOG_PERIOD) == 0)
	{
		double min = INFTY;
		unsigned int j =0;
		for(;j<THREADS;j++)
		{
			double tmp = array[j];
			if(tmp < min)
				min = tmp;
		}

		gettimeofday(&endTV, NULL);
		timersub(&endTV, &startTV, &diff);
		printf("%u - LOG %.10f %.2f/100.00 SEC:%d:%d\n", lid, min, ((double)ops_count[my_id])*100/OPERATIONS, (int)diff.tv_sec, (int)diff.tv_usec);
	}

	do
	{
		double timestamp = INFTY;
		unsigned int counter = 1;
		void* free_pointer;

		if(DATASTRUCT == 'N')
		{
			bucket_node *new = dequeue(nbqueue);
			free_pointer = new;
			timestamp = new->timestamp;
			counter = new->counter;
		}
		else if(DATASTRUCT == 'L')
		{
			free_pointer = list_pop(lqueue);
			bucket_node *new = node_payload(lqueue,free_pointer);
			if(free_pointer != NULL)
			{
				timestamp = new->timestamp;
				counter = new->counter;
			}
		}
		else if(DATASTRUCT == 'C')
		{
			calqueue_node* new = calqueue_get();
			free_pointer = new;
			if(free_pointer != NULL)
			{
				timestamp = new->timestamp;
				counter = 1;
			}
		}
		if(timestamp == INFTY)
		{
			if( VERBOSE )
				test_log(my_id, "%u-%d:%d\tDEQUEUE EMPTY\n", my_id, diff.tv_sec, diff.tv_usec);
			break;
		}
		else
		{
			n_dequeue++;

			if( VERBOSE )
				test_log(my_id, "%u-%d:%d\tDEQUEUE %.15f - %d\n", my_id, diff.tv_sec, diff.tv_usec, timestamp, counter);

			array[my_id] = timestamp;
			local_min = timestamp;
		}
		if(counter == 0)
		{
			printf("%u-%d:%d\tDEQUEUE should never return a HEAD node %.10f - %d\n",
					my_id, (int)diff.tv_sec, (int)diff.tv_usec, timestamp, counter);
			exit(1);
		}

		if(free_pointer != NULL)
			mm_free(free_pointer);
	}while(EMPTY_QUEUE);

	gettimeofday(&endTV, NULL);
	timersub(&endTV, &startTV, &diff);

	mm_get_log_data(&malloc_time[my_id], &malloc_count[my_id], &free_time[my_id], &free_count[my_id]);

	if(LOG)
		printf("%u- DONE + %d:%d %lld, %lld, %lld"
			" - MALLOC + %d:%d, %d"
			" - FREE + %d:%d, %d\n",
			my_id, (int)diff.tv_sec, (int)diff.tv_usec, n_dequeue, n_enqueue, n_dequeue - n_enqueue,
			(int)malloc_time[my_id].tv_sec, (int)malloc_time[my_id].tv_usec, malloc_count[my_id],
			(int)free_time[my_id].tv_sec, (int)free_time[my_id].tv_usec, free_count[my_id]);


	ops[my_id] = n_dequeue - n_enqueue;
	array[my_id] = INFTY;

	if( VERBOSE )
	{
		fclose(f);
		test_log(my_id,"%u- DONE + %d, %u, %u\n", my_id, (int)diff.tv_sec, n_dequeue, n_enqueue);
	}

	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	int par = 1;

	if(argc != 17)
	{
		printf("Missing parameters %d vs 16\n", argc);
		exit(1);
	}


	DATASTRUCT = argv[par++][0];
	TOTAL_OPS = (unsigned int) strtol(argv[par++], (char **)NULL, 10);
	THREADS  = (unsigned int) strtol(argv[par++], (char **)NULL, 10);
	OPERATIONS = (TOTAL_OPS/THREADS);
	PRUNE_PERIOD = (unsigned int) strtol(argv[par++], (char **)NULL, 10);
	PROB_ROLL = strtod(argv[par++], (char **)NULL);
	PROB_DEQUEUE = strtod(argv[par++], (char **)NULL);
	MEAN_INTERARRIVAL_TIME = strtod(argv[par++], (char **)NULL);
	PROB_DISTRIBUTION = argv[par++][0];
	LOG_PERIOD = (OPERATIONS/10);
	INIT_SIZE = (unsigned int) strtol(argv[par++], (char **)NULL, 10);
	VERBOSE = (unsigned int) strtol(argv[par++], (char **)NULL, 10);
	LOG = (unsigned int) strtol(argv[par++], (char **)NULL, 10);
	PRUNE_TRESHOLD = strtod(argv[par++], (char **)NULL);
	BUCKET_WIDTH = strtod(argv[par++], (char **)NULL);
	COLLABORATIVE_TODO_LIST = (unsigned int) strtol(argv[par++], (char **)NULL, 10);
	SAFETY_CHECK = (unsigned int) strtol(argv[par++], (char **)NULL, 10);
	EMPTY_QUEUE = (unsigned int) strtol(argv[par++], (char **)NULL, 10);

	id = (unsigned int*) malloc(THREADS*sizeof(unsigned int));
	ops = (long long*) malloc(THREADS*sizeof(long long));
	ops_count = (long long*) malloc(THREADS*sizeof(long long));
	malloc_time = (struct timeval*) malloc(THREADS*sizeof(struct timeval));
	free_time = (struct timeval*) malloc(THREADS*sizeof(struct timeval));
	malloc_count = (unsigned int*) malloc(THREADS*sizeof(unsigned int));
	free_count = (unsigned int*) malloc(THREADS*sizeof(unsigned int));
	array = (double*) malloc(THREADS*sizeof(double));
	log_files = (FILE**) malloc(THREADS*sizeof(FILE*));



//printf("####\n");
printf("D:%c,", DATASTRUCT);
printf("T:%u,", THREADS);
printf("OPS:%u,", TOTAL_OPS);
//printf("OPSpT:%u\n", OPERATIONS);
printf("PRUNE_PER:%u,", PRUNE_PERIOD);
printf("PRUNE_T:%f,", PRUNE_TRESHOLD);
//printf("PROB_ROLL:%f\n", PROB_ROLL);
printf("P_DEQUEUE:%f,", PROB_DEQUEUE);
printf("MEAN_INTERARRIVAL_TIME:%f,", MEAN_INTERARRIVAL_TIME);
printf("PROB_DIST:%c,",PROB_DISTRIBUTION);
//printf("LOG_PERIOD:%u\n", LOG_PERIOD);
printf("SIZE:%u,", INIT_SIZE);
printf("B_WIDTH:%f,", BUCKET_WIDTH);
printf("COLLABORATIVE_TODO_LIST:%u,", COLLABORATIVE_TODO_LIST);
printf("SAFETY_CHECK:%u,", SAFETY_CHECK);
printf("EMPTY_QUEUE:%u,", EMPTY_QUEUE);


	unsigned int i = 0;
	pthread_t tid[THREADS];

	mm_init(512, sizeof(bucket_node), false);

	if(DATASTRUCT == 'N')
		nbqueue = queue_init(INIT_SIZE, BUCKET_WIDTH, COLLABORATIVE_TODO_LIST);
	else if(DATASTRUCT == 'L')
	{
		lqueue = new_list(bucket_node);
		pthread_spin_init(&(((struct rootsim_list*)lqueue)->spinlock), 0);
	}
	else if(DATASTRUCT == 'C')
		calqueue_init();

	gettimeofday(&startTV, NULL);


	for(;i<THREADS;i++)
	{
		id[i] = i;
		ops_count[i] = 0;
		pthread_create(tid +i, NULL, process, id+i);
	}

	for(i=0;i<THREADS;i++)
		pthread_join(tid[i], (void*)&id);

	long long tmp = 0;
	struct timeval mal,fre;
	timerclear(&mal);
	timerclear(&fre);

	for(i=0;i<THREADS;i++)
	{
		tmp += ops[i];

		mal.tv_sec+=malloc_time[i].tv_sec;
		if( (mal.tv_usec + malloc_time[i].tv_usec )%1000000 != mal.tv_usec + malloc_time[i].tv_usec )
			mal.tv_sec++;
		mal.tv_usec = (mal.tv_usec + malloc_time[i].tv_usec )%1000000;

		fre.tv_sec+=free_time[i].tv_sec;
		if( (fre.tv_usec + free_time[i].tv_usec )%1000000 != fre.tv_usec + free_time[i].tv_usec )
			fre.tv_sec++;
		fre.tv_usec = (fre.tv_usec + free_time[i].tv_usec )%1000000;
	}


	printf("CHECK:%lld,", tmp);
	printf("MALLOC_T:%d.%d,", (int)mal.tv_sec, (int)mal.tv_usec);
	printf("FREE_T:%d.%d,", (int)fre.tv_sec, (int)fre.tv_usec);

	for(i=0;i<THREADS;i++)
		printf("%d:%lld,", i,ops_count[i]);

	return 0;
}
