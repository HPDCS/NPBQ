/*
 * rand.c
 *
 *  Created on: Dec 12, 2015
 *      Author: romolo
 */


#include <stdlib.h>
#include <math.h>

double uniform_rand(struct drand48_data *seed, double mean)
{
	double random_num = 0.0;
	drand48_r(seed, &random_num);
	random_num *= mean*2;
	return random_num;
}

double triangular_rand(struct drand48_data *seed, double mean)
{
	double random_num = 0.0;
	drand48_r(seed, &random_num);
	random_num = sqrt(random_num);
	random_num *= mean*3/2;
	return random_num;
}

double exponential_rand(struct drand48_data *seed, double mean)
{
	double random_num = 0.0;
	drand48_r(seed, &random_num);
	random_num =  -log(random_num);
	random_num *= mean;
	return random_num;
}
