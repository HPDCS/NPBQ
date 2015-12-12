/*
 * util.c
 *
 *  Created on: Dec 12, 2015
 *      Author: romolo
 */

#include <time.h>
#include <stdbool.h>
#include <stdlib.h>

__thread int failure;
__thread struct drand48_data seed;
__thread time_t MAX_BACKOFF = 1024;
__thread int attempt = 0;
__thread time_t SLOT = 16;

static void spin(time_t nanoseconds)
{
	// busy wait for 10 microseconds
	time_t nsec;

	nanoseconds = nanoseconds ^ ( (1000000000 ^ nanoseconds) & (-( 1000000000 < nanoseconds ) ) );

	struct timespec ttime,curtime;

	// get the time
	clock_gettime(CLOCK_REALTIME,&ttime);

	// loop
	do
	{
		  clock_gettime(CLOCK_REALTIME,&curtime);
		  time_t seconds = curtime.tv_sec - ttime.tv_sec;
		  nsec    = seconds*1000000000 + curtime.tv_nsec - ttime.tv_nsec;

	}while(nsec < nanoseconds);


}

void exponential_backoff()
{
	time_t 	nanoseconds = SLOT << attempt;
	nanoseconds = nanoseconds ^ ( (MAX_BACKOFF ^ nanoseconds) & (-( MAX_BACKOFF < nanoseconds ) ) );
	spin(nanoseconds);
}

void increase_attempt()
{
	attempt++;
}

void reset_attempts()
{
	attempt = 0;
}
