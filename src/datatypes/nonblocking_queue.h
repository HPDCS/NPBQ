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
/*
 * nonblockingqueue.h
 *
 *  Created on: Jul 13, 2015
 *      Author: Romolo Marotta
 */
#ifndef DATATYPES_NONBLOCKING_QUEUE_H_
#define DATATYPES_NONBLOCKING_QUEUE_H_

#include <stdbool.h>
#include <float.h>

#define INFTY DBL_MAX
#define D_EQUAL(a,b) (fabs((a) - (b)) < DBL_EPSILON)

/**
 *  Struct that define a node in a bucket
 *  */
typedef struct _bucket_node bucket_node;
struct _bucket_node
{
	//char pad1[64];
	bucket_node * volatile next;	// pointer to the successor
	char pad2[56];
	//void *queue;	// pointer to the successor
	void *payload;  				// general payload
	double timestamp;  				// key
	unsigned int counter; 			// used to resolve the conflict with same timestamp using a FIFO policy
	//char pad3[36];					// actually used only to distinguish head nodes
};

/**
 *
 */

typedef struct nonblocking_queue nonblocking_queue;
struct nonblocking_queue
{
	//char pad1[64];
	volatile unsigned long long current;
	char pad2[56];
	bucket_node * volatile future_list;
	char pad3[56];
	//volatile unsigned int starting_slot;
	//char pad4[60];
	//volatile unsigned int ending_slot;
	//char pad5[60];
	volatile unsigned int dequeue_size;
	//char pad6[60];
	volatile unsigned int collaborative_todo_list;
	char pad7[56];
	bucket_node * volatile todo_list;
	char pad8[56];

	//volatile bucket_node * volatile hashtable[32];
	bucket_node * volatile hashtable[32];

	char pad9[56];
	double bucket_width;
	bucket_node *tail;
	unsigned int init_size;
};


extern bool enqueue(nonblocking_queue *queue, double timestamp, void* payload);
extern bucket_node* dequeue(nonblocking_queue *queue);
extern double prune(nonblocking_queue *queue, double timestamp);
extern nonblocking_queue* queue_init(unsigned int size, double bucket_width, unsigned int collaborative_todo_list);
extern void queue_init_per_thread(unsigned int lid);

#endif /* DATATYPES_NONBLOCKING_QUEUE_H_ */
