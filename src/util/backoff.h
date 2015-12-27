/*
 * util.h
 *
 *  Created on: Dec 12, 2015
 *      Author: romolo
 */

#ifndef UTIL_BACKOFF_H_
#define UTIL_BACKOFF_H_

#include <stdlib.h>

extern void exponential_backoff(unsigned long long slot, unsigned long long attempt,
													struct drand48_data *backoff_seed);
extern unsigned int decrease_attempt(unsigned int attempt);
extern unsigned int increase_attempt(unsigned int attempt, unsigned int max_backoff);

#endif /* UTIL_BACKOFF_H_ */
