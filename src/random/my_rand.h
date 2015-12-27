/*
 * my_rand.h
 *
 *  Created on: Dec 12, 2015
 *      Author: romolo
 */

#ifndef RANDOM_MY_RAND_H_
#define RANDOM_MY_RAND_H_

#include <stdlib.h>

extern double uniform_rand(struct drand48_data *seed, double mean);
extern double triangular_rand(struct drand48_data *seed, double mean);
extern double exponential_rand(struct drand48_data *seed, double mean);

#endif /* RANDOM_MY_RAND_H_ */
