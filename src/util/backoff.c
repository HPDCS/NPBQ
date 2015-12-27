/*
 * util.c
 *
 *  Created on: Dec 12, 2015
 *      Author: romolo
 */

#include <time.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../random/my_rand.h"




#if defined(__i386__)

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#elif defined(__x86_64__)


#define CLOCK_READ() ({ \
			unsigned int lo; \
			unsigned int hi; \
			__asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi)); \
			((unsigned long long)hi) << 32 | lo; \
			})

//#endif /* _TIMER_H */

static __inline__ unsigned long long rdtsc(void)
{
  return CLOCK_READ();
}

#elif defined(__powerpc__)


static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int result=0;
  unsigned long int upper, lower,tmp;
  __asm__ volatile(
                "0:                  \n"
                "\tmftbu   %0           \n"
                "\tmftb    %1           \n"
                "\tmftbu   %2           \n"
                "\tcmpw    %2,%0        \n"
                "\tbne     0b         \n"
                : "=r"(upper),"=r"(lower),"=r"(tmp)
                );
  result = upper;
  result = result<<32;
  result = result|lower;

  return(result);
}

#endif


static void spin(unsigned long long nanoseconds)
{
	return;
	// busy wait for 10 microseconds
	time_t nsec;

	nanoseconds = nanoseconds ^ ( (1000000000 ^ nanoseconds) & (-( 1000000000 < nanoseconds ) ) );

	struct timespec ttime,curtime;
	nsec = 0;
	unsigned long long  a,b;
	a = rdtsc();
	b = a+nanoseconds;
	// get the time
	clock_gettime(CLOCK_THREAD_CPUTIME_ID,&ttime);

	// loop
	do
	{

		  clock_gettime(CLOCK_THREAD_CPUTIME_ID,&curtime);
		  time_t seconds = curtime.tv_sec - ttime.tv_sec;
		  nsec    = seconds*1000000000 + curtime.tv_nsec - ttime.tv_nsec;

		//b = rdtsc();

		//nsec++;
	}while(b-a < nanoseconds);
	//}while(rdtsc() < b);


}



void exponential_backoff(unsigned int slot,
							unsigned int attempt, struct drand48_data *backoff_seed)
{
	unsigned int k = (1U << attempt) -1;
	k = (unsigned int) (k*uniform_rand(backoff_seed, 0.5));
	unsigned long long 	nanoseconds = ((unsigned long long)slot) * ((unsigned long long)k);
	spin(nanoseconds);
}

unsigned int decrease_attempt(unsigned int attempt)
{
	unsigned int res = (unsigned int) (-(attempt != 0));
	return (attempt-1) & res;
}

unsigned int increase_attempt(unsigned int attempt, unsigned int max_backoff)
{
	attempt++;
	return attempt ^ ( (max_backoff ^ attempt) & ( (unsigned int) (-( max_backoff < attempt )) ) );
}
