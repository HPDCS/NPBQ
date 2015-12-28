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
 * nonblocking_queue.c
 *
 *  Created on: July 13, 2015
 *  Author: Romolo Marotta
 */


#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <float.h>
#include <pthread.h>
#include <math.h>

#include "../arch/atomic.h"
#include "../mm/myallocator.h"
#include "../datatypes/nonblocking_queue.h"

__thread bucket_node *to_free_pointers = NULL;
__thread unsigned int  lid;
__thread unsigned int  mark;

#define USE_MACRO 1

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

/**
 * This function calls a machine instruction that in O(1) finds the first set bit
 * of a 32bit value (Bit Scan Reverse)
 *
 * @author Romolo Marotta
 *
 * @param value
 *
 */
static inline unsigned int ibsr_x86(unsigned int value)
{
	unsigned int res = 0;

	__asm__ __volatile__(
			"bsrl %1, %0;"
			: "=r"(res)
			: "r"(value)
			: "cc"
	);

	return res & (-(value != 0));
}

/**
 *  This function computes the index for the first level hashtable given the linear index
 *  of the entry and the initial size of the hashtable
 *
 *  @author Romolo Marotta
 *
 *  @param index
 *  @param init_size
 *
 *  @return the first-level index
 */
#if USE_MACRO == 0
static inline unsigned int firstIndex(unsigned int index, unsigned int init_size)
{

	return ( (ibsr_x86(index) - ibsr_x86(init_size) + 1) & -( (index) >= (init_size) ) );
}
#else
#define firstIndex(index, init_size)\
		( (ibsr_x86(index) - ibsr_x86(init_size) + 1) & -( (index) >= (init_size) ) )
#endif

/**
 *  This function returns the pointer to the bucket corresponding to the given linear index
 *  for the first level hashtable given the linear index
 *  of the entry and the initial size of the hashtable
 *
 *  Based on code of D. Dechev, P. Pirkelbauer, and B. Stroustrup.
 *  For further information see:
 *  "Lock-free dynamically resizable arrays."
 *  OPODIS'06 Proceedings of the 10th international conference on Principles of Distributed Systems
 *
 *  @author Romolo Marotta
 *
 *  @param queue
 *  @param index
 *
 *  @return the bucket in the given queue corresponding to the index
 */
#if USE_MACRO == 0
static inline char* access_hashtable(volatile void *hashtable, unsigned int index,
		unsigned int init_size, unsigned int item_size)
{
	char **h = (char**) hashtable;
	unsigned int indbit = ibsr_x86(index);
	unsigned int findex = indbit - ibsr_x86(init_size) + 1;
	unsigned int check0 = (index >= init_size);
	findex &= -check0;
	return h[findex]+ (index & ((unsigned int)~(check0 << indbit)))*item_size;
}
#else
#define access_hashtable(hashtable, index, init_size, item_size)\
		(char*)({\
			char **h = (char**) (hashtable);\
			char* res;\
			unsigned int check0 = ( (index) >= (init_size) );\
			unsigned int indbit = ibsr_x86(index);\
			unsigned int findex = indbit - ibsr_x86(init_size) + 1;\
			findex &= -check0;\
			res = h[findex]+ ( (index) & ((unsigned int)~(check0 << indbit)))*(item_size);\
			res;\
		})
#endif

/**
 * This function computes the index of the destination bucket in the hashtable
 *
 * @author Romolo Marotta
 *
 * @param timestamp the value to be hashed
 * @param bucket_width the depth of a bucket
 *
 * @return the linear index of a given timestamp
 */
#if USE_MACRO == 0
static inline unsigned int hash(double timestamp, double bucket_width)
{
	return ((unsigned int) (timestamp / bucket_width));
}
#else
#define hash(timestamp, bucket_width)\
		((unsigned int) ( (timestamp) / (bucket_width) ))
#endif

/**
 *  This function returns an unmarked reference
 *
 *  @author Romolo Marotta
 *
 *  @param pointer
 *
 *  @return the unmarked value of the pointer
 */
#if USE_MACRO == 0
static inline void* get_unmarked(void *pointer)
{
	return (void*) (((unsigned long long) pointer) & ~((unsigned long long) 1));
}
#else
#define get_unmarked(pointer)\
	( (void*) (((unsigned long long) (pointer) ) & ~((unsigned long long) 1)) )
#endif

/**
 *  This function returns a marked reference
 *
 *  @author Romolo Marotta
 *
 *  @param pointer
 *
 *  @return the marked value of the pointer
 */
#if USE_MACRO == 0
static inline void* get_marked(void *pointer)
{
	return (void*) (((unsigned long long) pointer) | (1));
}
#else
#define get_marked(pointer)\
	( (void*) (((unsigned long long) (pointer) ) | (1)) )
#endif

/**
 *  This function checks if a reference is marked
 *
 *  @author Romolo Marotta
 *
 *  @param pointer
 *
 *  @return true if the reference is marked, else false
 */
#if USE_MACRO == 0
static inline bool is_marked(void *pointer)
{
	return (bool) (((unsigned long long) pointer) & 1);
}
#else
#define is_marked(pointer)\
	( (bool) (((unsigned long long) pointer) & 1) )
#endif

/**
* This function generates a mark value that is unique w.r.t. the previous values for each Logical Process.
* It is based on the Cantor Pairing Function, which maps 2 naturals to a single natural.
* The two naturals are the LP gid (which is unique in the system) and a non decreasing number
* which gets incremented (on a per-LP basis) upon each function call.
* It's fast to calculate the mark, it's not fast to invert it. Therefore, inversion is not
* supported at all in the code.
*
* @author Alessandro Pellegrini
*
* @param lid The local Id of the Light Process
* @return A value to be used as a unique mark for the message within the LP
*/
#if USE_MACRO == 0
static inline unsigned long long generate_mark() {
	unsigned long long k1 = lid;
	unsigned long long k2 = mark++;
	unsigned long long res = (unsigned long long)( ((k1 + k2) * (k1 + k2 + 1) / 2) + k2 );
	return ((~((unsigned long long)0))>>32) & res;
}
#else
#define generate_mark()\
	(unsigned long long) ({\
		unsigned long long k1 = lid;\
		unsigned long long k2 = mark++;\
		unsigned long long res = (unsigned long long)( ((k1 + k2) * (k1 + k2 + 1) >> 1) + k2 );\
		res = ((~((unsigned long long)0))>>32) & res ;\
		res;\
	 })
#endif

/**
 * This function blocks the execution of the process.
 * Used for debug purposes.
 * TODO remove it from code
 */
static void error(const char *msg, ...) {
	char buf[1024];
	va_list args;

	va_start(args, msg);
	vsnprintf(buf, 1024, msg, args);
	va_end(args);

	printf("%s", buf);
	exit(1);
}

/**
 * This function connect to a private structure marked
 * nodes in order to free them later, during a synchronisation point
 *
 * @author Romolo Marotta
 *
 * @param queue used to associate freed nodes to a queue
 * @param start the pointer to the first node in the disconnected sequence
 * @param end   the pointer to the last node in the disconnected sequence
 * @param timestamp   the timestamp of the last disconnected node
 *
 *
 */
#if USE_MACRO == 0
static inline void connect_to_be_freed_list(nonblocking_queue *queue, bucket_node *start, unsigned int counter)
{
	start->payload = to_free_pointers;
	//start->queue = queue;
	start->counter = counter;
	to_free_pointers = start;
}
#else
#define connect_to_be_freed_list(queue, start, counterm)\
{\
	(start)->payload = to_free_pointers;\
	/*(start)->queue   = (queue);*/\
	(start)->counter = (counterm);\
	to_free_pointers = (start);\
}
#endif


/**
 *  This function is an helper to allocate a node and filling its fields.
 *
 *  @author Romolo Marotta
 *
 *  @param payload is a pointer to the referred payload by the node
 *  @param timestamp the timestamp associated to the payload
 *
 *  @return the pointer to the allocated node
 *
 */
#if USE_MACRO == 0
static inline bucket_node* node_malloc(void *payload, double timestamp)
{

	bucket_node* res = (bucket_node*) mm_malloc();

	if (is_marked(res))
		error("%lu - Not aligned Node \n", pthread_self());

	res->counter = 1;
	res->next = NULL;
	res->payload = payload;
	res->timestamp = timestamp;

	return res;
}
#else
#define node_malloc(n_payload, n_timestamp)\
({\
	bucket_node* res = (bucket_node*) mm_malloc();\
	/*if (is_marked(res))\
		error("%lu - Not aligned Node \n", pthread_self());*/\
	res->counter = 1;\
	res->next = NULL;\
	res->generator = res;\
	res->to_remove = 0;\
	res->payload = (n_payload);\
	res->timestamp = (n_timestamp);\
	res;\
})
#endif


/**
 * This function implements the search of a node that contains a given timestamp t. It finds two adjacent nodes,
 * left and right, such that: left.timestamp <= t and right.timestamp > t.
 *
 * Based on the code by Timothy L. Harris. For further information see:
 * Timothy L. Harris, "A Pragmatic Implementation of Non-Blocking Linked-Lists"
 * Proceedings of the 15th International Symposium on Distributed Computing, 2001
 *
 * @author Romolo Marotta
 *
 * @param queue the queue that contains the bucket
 * @param head the head of the list in which we have to perform the search
 * @param timestamp the value to be found
 * @param left_node a pointer to a pointer used to return the left node
 * @param right_node a pointer to a pointer used to return the right node
 *
 */
static void search(nonblocking_queue* queue, bucket_node *head, double timestamp,
		bucket_node **left_node, bucket_node **right_node)
{
	bucket_node *left, *right, *left_next, *tmp, *tmp_next, *tail;
	unsigned int counter;
	tail = queue->tail;

	do
	{
		// Fetch the head and its next
		tmp = head;
		tmp_next = tmp->next;
		counter = 0;
		do
		{
			bool marked = is_marked(tmp_next);
			// Find the first unmarked node that is <= timestamp
			if (!marked)
			{
				left = tmp;
				left_next = tmp_next;
				counter = 0;
			}
			// Take a snap to identify the last marked node before the right node
			counter+=marked;

			// Find the next unmarked node from the left node (right node)
			tmp = get_unmarked(tmp_next);
			//if (tmp == tail)
			//	break;
			tmp_next = tmp->next;

		} while (	tmp != tail &&
					(
						is_marked(tmp_next)
						|| tmp->timestamp < timestamp
						|| D_EQUAL(tmp->timestamp, timestamp)
					)
				);

		// Set right node
		right = tmp;

		//left node and right node have to be adjacent. If not try with CAS
		if (left_next != right)
		{
			// if CAS succeeds connect the removed nodes to to_be_freed_list
			if (!CAS_x86(
						(volatile unsigned long long *)&(left->next),
						(unsigned long long) left_next,
						(unsigned long long) right
						)
					)
				continue;
			connect_to_be_freed_list(queue, left_next, counter);
		}
		// at this point they are adjacent. Thus check that right node is still unmarked and return
		if (right == tail || !is_marked(right->next))
		{
			*left_node = left;
			*right_node = right;
			return;
		}
	} while (1);
}

/**
 * This function commits a value in the current field of a queue. It retries until the timestamp
 * associated with current is strictly less than the value that has to be committed
 *
 * @author Romolo Marotta
 *
 * @param queue the interested queue
 * @param left_node the candidate node for being next current
 *
 */
static inline void flush_current(nonblocking_queue* queue, unsigned int index, bucket_node* node)
{
	unsigned long long oldCur;
	unsigned int oldIndex;
	unsigned long long newCur =  ( ( unsigned long long ) index ) << 32;
	newCur |= generate_mark();

	// Retry until the left node has a timestamp strictly less than current and
	// the CAS fails
	do
	{

		oldCur = queue->current;
		oldIndex = (unsigned int) (oldCur >> 32);
	}
	while (
			index <= oldIndex && !is_marked(node->next)
			&& !CAS_x86(
						(volatile unsigned long long *)&(queue->current),
						(unsigned long long) oldCur,
						(unsigned long long) newCur
						)
					);
}


/**
 * This function commits a value in the current field of a queue. It retries until the timestamp
 * associated with current is strictly less than the value that has to be committed
 *
 * @author Romolo Marotta
 *
 * @param queue the interested queue
 * @param left_node the candidate node for being next current
 *
 */
static inline void smart_flush_current(nonblocking_queue* queue, unsigned int index, bucket_node* node, bool was_empty)
{
	unsigned long long oldCur1;
	unsigned long long oldCur2;
	unsigned int oldIndex1;
	unsigned int oldIndex2;
	unsigned long long newCur =  ( ( unsigned long long ) index ) << 32;
	newCur |= generate_mark();

	// Retry until the left node has a timestamp strictly less than current and
	// the CAS fails

	oldCur1 = queue->current;
	oldIndex1 = (unsigned int) (oldCur1 >> 32);

	if(		was_empty &&
			index <= oldIndex1 && !is_marked(node->next)
			&& !CAS_x86(
						(volatile unsigned long long *)&(queue->current),
						(unsigned long long) oldCur1,
						(unsigned long long) newCur
						)
					)

	{

		do
		{
			oldCur2 = queue->current;
			oldIndex2 = (unsigned int) (oldCur2 >> 32);
		}
		while(
					(index < oldIndex2)
					&& !is_marked(node->next)
					&& !CAS_x86(
								(volatile unsigned long long *)&(queue->current),
								(unsigned long long) oldCur2,
								(unsigned long long) newCur
								)
							);

	}

}



/**
 * This function insert a new event in the nonblocking queue.
 * The cost of this operation when succeeds should be O(1) as calendar queue
 *
 * @author Romolo Marotta
 *
 * @param queue the interested queue
 * @param timestamp the timestamp of the event
 * @param payload the event to be enqueued
 *
 */
static bool insert(nonblocking_queue* queue, bucket_node* new_node)
{
	bucket_node *left_node, *right_node, *tmp_node, *tmp, *bucket;
	unsigned int index;
	unsigned int tmp_size;
	bool cas_result = false;

	// Phase 1. Check if the hashtable cover the timestamp value. If not add the event in future list
	do
	{
		// if the next of the future list head is null it means
		// that an expansion of the hashtable is occurring
		//do
		{
			tmp_node = queue->future_list;
			tmp_size = tmp_node->counter;
			tmp = tmp_node->next;
			new_node->next = tmp;
		}
		//while(is_marked(tmp));
		if(is_marked(tmp))
			break;

		index = hash(new_node->timestamp, queue->bucket_width);
	} while (index >= tmp_size
			&& !(cas_result = CAS_x86(
								(volatile unsigned long long *)&(tmp_node->next),
								(unsigned long long) tmp,
								(unsigned long long) new_node
								)
				)
			);

	// node connected in future list
	if (cas_result)
		return false;

	// node to be added in the hashtable
	bucket = (bucket_node*)access_hashtable(queue->hashtable, index, queue->init_size, sizeof(bucket_node));

	do
	{
		search(queue, bucket, new_node->timestamp, &left_node,
				&right_node);
		new_node->next = right_node;
		new_node->counter = 1 + ( -D_EQUAL(new_node->timestamp, right_node->timestamp ) & right_node->counter );
	} while (!CAS_x86(
				(volatile unsigned long long*)&(left_node->next),
				(unsigned long long) right_node,
				(unsigned long long) new_node
				)
			);
	return true;
}


/**
 * This function insert a new event in the nonblocking queue.
 * The cost of this operation when succeeds should be O(1) as calendar queue
 *
 * @author Romolo Marotta
 *
 * @param queue the interested queue
 * @param timestamp the timestamp of the event
 * @param payload the event to be enqueued
 *
 */
static bool smart_insert(nonblocking_queue* queue, bucket_node* new_node, bool *was_empty)
{
	bucket_node *left_node, *right_node, *tmp_node, *tmp, *bucket;
	unsigned int index;
	unsigned int tmp_size;
	bool cas_result = false;

	// Phase 1. Check if the hashtable cover the timestamp value. If not add the event in future list
	do
	{
		// if the next of the future list head is null it means
		// that an expansion of the hashtable is occurring
		//do
		{
			tmp_node = queue->future_list;
			tmp_size = tmp_node->counter;
			tmp = tmp_node->next;
			new_node->next = tmp;
		}
		//while(is_marked(tmp));
		if(is_marked(tmp))
			break;

		index = hash(new_node->timestamp, queue->bucket_width);
	} while (index >= tmp_size
			&& !(cas_result = CAS_x86(
								(volatile unsigned long long *)&(tmp_node->next),
								(unsigned long long) tmp,
								(unsigned long long) new_node
								)
				)
			);

	// node connected in future list
	if (cas_result)
		return false;

	// node to be added in the hashtable
	bucket = (bucket_node*)access_hashtable(queue->hashtable, index, queue->init_size, sizeof(bucket_node));

	do
	{
		search(queue, bucket, new_node->timestamp, &left_node,
				&right_node);
		*was_empty = left_node == bucket && right_node == queue->tail;
		new_node->next = right_node;
		new_node->counter = 1 + ( -D_EQUAL(new_node->timestamp, right_node->timestamp ) & right_node->counter );
	} while (!CAS_x86(
				(volatile unsigned long long*)&(left_node->next),
				(unsigned long long) right_node,
				(unsigned long long) new_node
				)
			);
	return true;
}

/**
 * This function implements the search of a node that contains a given timestamp t. It finds two adjacent nodes,
 * left and right, such that: left.timestamp <= t and right.timestamp > t.
 *
 * Based on the code by Timothy L. Harris. For further information see:
 * Timothy L. Harris, "A Pragmatic Implementation of Non-Blocking Linked-Lists"
 * Proceedings of the 15th International Symposium on Distributed Computing, 2001
 *
 * @author Romolo Marotta
 *
 * @param queue the queue that contains the bucket
 * @param head the head of the list in which we have to perform the search
 * @param timestamp the value to be found
 * @param left_node a pointer to a pointer used to return the left node
 * @param right_node a pointer to a pointer used to return the right node
 *
 */
static bool try_search(nonblocking_queue* queue, bucket_node *head, double timestamp, void* payload,
		bucket_node **left_node, bucket_node **right_node)
{
	bucket_node *left, *right, *left_next, *tmp, *tmp_next, *tail;
	unsigned int counter;
	tail = queue->tail;

	do
	{
		// Fetch the head and its next
		tmp = head;
		tmp_next = tmp->next;
		counter = 0;
		do
		{
			bool marked = is_marked(tmp_next);
			// Find the first unmarked node that is <= timestamp
			if (!marked)
			{
				left = tmp;
				left_next = tmp_next;
				counter = 0;
			}
			// Take a snap to identify the last marked node before the right node
			counter+=marked;

			// Find the next unmarked node from the left node (right node)
			tmp = get_unmarked(tmp_next);
			//if (tmp == tail)
			//	break;
			tmp_next = tmp->next;
			if(tmp->generator ==  payload)
				return false;

		} while (	tmp != tail &&
					(
						is_marked(tmp_next)
						|| tmp->timestamp < timestamp
						|| D_EQUAL(tmp->timestamp, timestamp)
					)
				);

		// Set right node
		right = tmp;

		//left node and right node have to be adjacent. If not try with CAS
		if (left_next != right)
		{
			// if CAS succeeds connect the removed nodes to to_be_freed_list
			if (!CAS_x86(
						(volatile unsigned long long *)&(left->next),
						(unsigned long long) left_next,
						(unsigned long long) right
						)
					)
				continue;
			connect_to_be_freed_list(queue, left_next, counter);
		}
		// at this point they are adjacent. Thus check that right node is still unmarked and return
		if (right == tail || !is_marked(right->next))
		{
			*left_node = left;
			*right_node = right;
			return true;
		}
	} while (1);
}


/**
 * This function tries to remove one element from the top of the todo_list
 * end try to insert it in the hashtable
 *
 * @author Romolo Marotta
 *
 * @param queue the queue of which the todo_list is taken
 *
 * @return true if the thread removes the last event in the todo_list
 *
 */
static void try_empty_todo_list(nonblocking_queue* queue)
{
	bucket_node *tmp, *tmp_next, *tail, *head, *tmp_node, *bucket, *left_node, *right_node, *tmp_ftr;
	unsigned int tmp_size;
	unsigned int index;
	bool cas_result = false;
	tail = queue->tail;
	head = queue->todo_list;


	bucket_node *new_node = node_malloc(NULL, 0.0);

	// Phase 1. Check if the hashtable cover the timestamp value. If not add the event in future list
	do
	{
		tmp = get_unmarked(head->next);
		tmp_next = tmp->next;
		if(tmp->to_remove == 1)
		{
			mm_free(new_node);
			goto end;
		}
		if(tmp == tail)
		{
			mm_free(new_node);
			return;
		}// if the next of the future list head is null it means
		// that an expansion of the hashtable is occurring

		new_node->timestamp = tmp->timestamp;
		new_node->payload = tmp->payload;
		new_node->generator = tmp->generator;
		//do
		{
			tmp_node = queue->future_list;
			tmp_size = tmp_node->counter;
			tmp_ftr = tmp_node->next;
			new_node->next = tmp_ftr;
		}
		//while(is_marked(tmp_ftr));
		if(is_marked(tmp_ftr))
			break;
		index = hash(new_node->timestamp, queue->bucket_width);
	} while (index >= tmp_size
			&& !(cas_result = CAS_x86(
								(volatile unsigned long long *)&(tmp_node->next),
								(unsigned long long) tmp_ftr,
								(unsigned long long) new_node
								)
				)
			);


	// node connected in future list
	if (!cas_result)
	{
		// node to be added in the hashtable
		bucket = (bucket_node*)access_hashtable(queue->hashtable, index, queue->init_size, sizeof(bucket_node));

		bool res = false;
		do
		{
			res = try_search(queue, bucket, new_node->timestamp, new_node->generator, &left_node,
					&right_node);
			new_node->next = right_node;
			new_node->counter = 1 + ( -D_EQUAL(new_node->timestamp, right_node->timestamp ) & right_node->counter );
		} while (res && !CAS_x86(
					(volatile unsigned long long*)&(left_node->next),
					(unsigned long long) right_node,
					(unsigned long long) new_node
					)
				);

		if(!res)
			mm_free(new_node);
	}


	end:
	// Try to disconnect the head node
	if(CAS_x86(
				(volatile unsigned long long *)&(head->next),
				(unsigned long long) get_marked(tmp),
				(unsigned long long) get_marked(tmp_next)
			)
		)
	{
		connect_to_be_freed_list(queue, tmp, 1);
		tmp->to_remove = 1;
	}
	else
	{
		if(cas_result)
			new_node->to_remove = 1;
	}

}


/**
 * This function tries to remove one element from the top of the todo_list
 * end try to insert it in the hashtable
 *
 * @author Romolo Marotta
 *
 * @param queue the queue of which the todo_list is taken
 *
 * @return true if the thread removes the last event in the todo_list
 *
 */
static void empty_todo_list(nonblocking_queue* queue)
{
	bucket_node *tmp, *tmp_next, *tail, *head;
	tail = queue->tail;
	head = queue->todo_list;

	// Try to disconnect the head node
	do
	{
		tmp = get_unmarked(head->next);
		tmp_next = tmp->next;
	} while (tmp != tail && !CAS_x86(
				(volatile unsigned long long *)&(head->next),
				(unsigned long long) get_marked(tmp),
				(unsigned long long) get_marked(tmp_next)
				)
			);

	//insert in the hashtable or in the future list again
	if(tmp != tail)
		insert(queue, tmp);
}

/**
 * This function expand the hashtable of a queue without relocating the array.
 *
 * @author Romolo Marotta
 *
 * @param queue is the queue to be expanded
 * @param old_size is the size of the hashtable when an expansion is required
 *
 * @return true if before it ends the dequeue size is increased
 */
static bool expand_array(nonblocking_queue* queue, volatile unsigned int old_size)
{
	unsigned int i;
	bucket_node *tail, *new_future, *future;
	bucket_node *tmp, *tmp_next;


	if(queue->dequeue_size != old_size)
		return queue->dequeue_size > old_size;

	tail = queue->tail;

	future = queue->future_list;
	if(future->counter == old_size)
	{
		// Alloc new hashtable
		bucket_node *tmp_new_heads = (bucket_node*) mm_std_malloc(sizeof(bucket_node) * old_size);

		if(tmp_new_heads == NULL)
			error("No enough memory to allocate new hashtable");

		bzero(tmp_new_heads, sizeof(bucket_node) * old_size);

		// Copy the unchanged entries
		for (i = 0; i < old_size; i++)
		{
			tmp_new_heads[i].timestamp = (old_size+i) * queue->bucket_width;
			tmp_new_heads[i].counter = 0;
			tmp_new_heads[i].next = tail;
		}
		// Init the head of new lists

		if( CAS_x86(
				(unsigned long long*) &(queue->hashtable[firstIndex(old_size, queue->init_size)]),
				(unsigned long long)  NULL,
				(unsigned long long)  tmp_new_heads
				)
		)
		{
			//printf("%u - EXPAND START %u to %u\n", lid, old_size, old_size*2);
		}
		else
			mm_free(tmp_new_heads);

		tmp = queue->todo_list;
		if(tmp->counter < old_size)

			if(CAS_x86(
					(unsigned long long*) &queue->todo_list,
					(unsigned long long)  tmp,
					(unsigned long long)  future
				))
					connect_to_be_freed_list(queue, tmp, 1);


		new_future = node_malloc(NULL, -4.0);
		new_future->counter = old_size*2;
		new_future->next = tail;


		if(!CAS_x86(
				(unsigned long long*) &queue->future_list,
				(unsigned long long)  future,
				(unsigned long long)  new_future
				)
		)
			mm_free(new_future);


	}

	do
	{
		tmp = queue->todo_list;
		tmp_next = tmp->next;
	}
	while(!is_marked(tmp_next)
			&& !CAS_x86(
					(unsigned long long*) &tmp->next,
					(unsigned long long)  tmp_next,
					(unsigned long long)  get_marked(tmp_next)
			)
	);

	// empty the to do list
	tmp = queue->todo_list->next;

	while (get_unmarked(tmp) != tail)
	{
		empty_todo_list(queue);
		tmp = queue->todo_list->next;
	}


	iCAS_x86(&queue->dequeue_size, old_size, old_size*2);
	return queue->dequeue_size > old_size;
}

/**
 * This function create an instance of a non-blocking calendar queue.
 *
 * @author Romolo Marotta
 *
 * @param queue_size is the inital size of the new queue
 *
 * @return a pointer a new queue
 */
nonblocking_queue* queue_init(unsigned int queue_size, double bucket_width, unsigned int collaborative_todo_list)
{
	unsigned int i = 0;

	nonblocking_queue* res = (nonblocking_queue*) mm_std_malloc(sizeof(nonblocking_queue));
	if(res == NULL)
		error("No enough memory to allocate queue\n");
	memset(res, 0, sizeof(nonblocking_queue));

	res->hashtable[0] = (bucket_node*) mm_std_malloc(sizeof(bucket_node) * queue_size);
	if(res->hashtable == NULL)
	{
		mm_std_free(res);
		error("No enough memory to allocate queue\n");
	}
	memset(res->hashtable[0], 0, sizeof(bucket_node) * queue_size);

	res->dequeue_size = queue_size;
	res->tail = node_malloc(NULL, -4.0);
	res->tail->next = NULL;
	res->tail->counter = 0;
	res->bucket_width = bucket_width;
	res->future_list = node_malloc(NULL, -4.0);
	res->future_list->counter = queue_size;
	res->future_list->next = res->tail;
	res->todo_list = node_malloc(NULL, -4.0);
	res->todo_list->counter = 0;
	res->todo_list->next = get_marked(res->tail);
	res->current = ((unsigned long long) queue_size-1) << 32;
	res->collaborative_todo_list = collaborative_todo_list;
	res->init_size = queue_size;

	for (i = 0; i < queue_size; i++)
	{
		res->hashtable[0][i].next = res->tail;
		res->hashtable[0][i].timestamp = i * bucket_width;
		res->hashtable[0][i].counter = 0;
	}

	return res;
}

/**
 * This function implements the enqueue interface of the non-blocking queue.
 * Should cost O(1) when succeeds
 *
 * @author Romolo Marotta
 *
 * @param queue
 * @param timestamp the key associated with the value
 * @param payload the event to be enqueued
 *
 * @return true if the event is inserted in the hashtable, else false
 */
bool enqueue(nonblocking_queue* queue, double timestamp, void* payload)
{
	// allocates a new node
	bool was_empty = false;
	bucket_node *new_node = node_malloc(payload, timestamp);
	bool res = smart_insert(queue, new_node, &was_empty);
	// Try to flush the new current if necessary
	if(res)
		smart_flush_current(queue, hash(new_node->timestamp, queue->bucket_width), new_node, was_empty);

	// Collaborate in emptying the todo_list

	// empty the to do list
	if(queue->collaborative_todo_list)
	{
		bucket_node *tmp;
		bucket_node *tail;

		tail = queue->tail;
		tmp = queue->todo_list;
		tmp = tmp->next;
		while (get_unmarked(tmp) != tail)
		{
			empty_todo_list(queue);
			tmp = queue->todo_list;
			tmp = tmp->next;
		}
	}
	return res;
}

/**
 * This function dequeue from the nonblocking queue. The cost of this operation when succeeds should be O(1)
 *
 * @author Romolo Marotta
 *
 * @param queue the interested queue
 *
 * @return a pointer to a node that contains the dequeued value
 *
 */
bucket_node* dequeue(nonblocking_queue *queue)
{
	bucket_node *right_node, *min, *min_next, *right_node_next, *candidate, *res, *tail;
	unsigned int index;
	unsigned int tmp_size;
	unsigned int to_remove_counter;
	unsigned long long oldCurrent;

	tail = queue->tail;
	res = NULL;
	do
	{
		// 1. Check if there are no events
		oldCurrent = queue->current;
		index = (unsigned int)(oldCurrent >> 32);
		min = (bucket_node*) access_hashtable(queue->hashtable, index, queue->init_size, sizeof(bucket_node));
		to_remove_counter = 0;
		right_node_next = (bucket_node*)0xDEADC0DE;
		// 2. Check if current is marked and find left node
		min_next = min->next;

		// 3. Find right node
		right_node = min_next;
		//if(right_node != tail)
		{
			right_node_next = right_node->next;

			while (is_marked(right_node_next))
			{

				//printf("%u - LOOP R:%p RN:%p, T:%p TN:%p I:%u M:%p MN:%p\n", lid, right_node, right_node_next, tail, tail->next, index, min, min_next);
				to_remove_counter++;

				right_node = get_unmarked(right_node_next);

			//	if(right_node == tail)
				//	break;
				right_node_next = right_node->next;
			}
		}
		// 4. If left and right are not adjacent try to make them else restart
		if (min_next != right_node)
		{
			if (!CAS_x86(
					(volatile unsigned long long *)&(min->next),
					(unsigned long long) min_next,
					(unsigned long long) right_node
					)
				)
				continue;
			connect_to_be_freed_list(queue, min_next, to_remove_counter);
		}

		candidate = right_node;
		//printf("%u - CHECK R:%p RN:%p, T:%p TN:%p I:%u M:%p MN:%p\n", lid, right_node, right_node_next, tail, tail->next, index, min, min_next);
		// 5. Right node is a tail.
		if (candidate == queue->tail)
		{
			// 7. get next bucket
			tmp_size = queue->dequeue_size;
			//index = hash(min->timestamp, queue->bucket_width) + 1;
			index++;
			// 8. Find new right node, that should be a head.
			if (index < tmp_size)
				candidate = (bucket_node*)access_hashtable(queue->hashtable, index, queue->init_size, sizeof(bucket_node));
			else
			{
				bucket_node *future = queue->future_list;

				if (future->next == tail && future->counter == tmp_size)
				{
					res = node_malloc(NULL, INFTY);
					return res;
				}

				else if(expand_array(queue, tmp_size))
					candidate = (bucket_node*)access_hashtable(queue->hashtable, index, queue->init_size, sizeof(bucket_node));
				else
					continue;
			}

		}
		//printf("%u - CHECK2 R:%p RN:%p, T:%p TN:%p I:%u M:%p MN:%p\n", lid, candidate, NULL, tail, tail->next, index, min, min_next);

		// 6. If the right node is not a tail // Try to update current and then Check that nothing is changed
		// 9. Set current temporarily to the right node
		// 10. Nothing is changed

		if( candidate->counter != 0 )
		{
			res = node_malloc(candidate, candidate->timestamp);
			res->counter = candidate->counter;
			// 11. Something changed, thus restore current
			if(CAS_x86(
					(volatile unsigned long long *)&(candidate->next),
					(unsigned long long) right_node_next,
					(unsigned long long) get_marked(right_node_next)
					)
				)
			{
				//printf("%u - CAN OK %p\n", lid, candidate);
				return res;
			}

			mm_free(res);
			res = NULL;
		}

		else
			CAS_x86(
					(volatile unsigned long long *)&(queue->current),
					(unsigned long long)oldCurrent,
					( ( (unsigned long long) index ) << 32) | generate_mark()
					//(((unsigned long long)hash(candidate->timestamp, queue->bucket_width)) << 32)
					)
				;

	}while(1);
	return NULL;
}

/**
 * This function frees any node in the hashtable with a timestamp strictly less than a given threshold,
 * assuming that any thread does not hold any pointer related to any nodes
 * with timestamp lower than the threshold.
 *
 * @author Romolo Marotta
 *
 * @param queue the interested queue
 * @param timestamp the threshold such that any node with timestamp strictly less than it is removed and freed
 *
 */
double prune(nonblocking_queue *queue, double timestamp)
{
	unsigned int end_index = hash(timestamp, queue->bucket_width);
	unsigned int start_index = 0;//queue->starting_slot;
	unsigned int i;
	double committed = 0;
	bucket_node *tmp, *to_remove_node;
	bucket_node* tail = queue->tail;
	bucket_node **tmp_previous = &to_free_pointers;
	unsigned int counter;

	for (i = start_index; i < end_index; i++)
	{
		bucket_node* head = (bucket_node*)access_hashtable(queue->hashtable, i, queue->init_size, sizeof(bucket_node));

		to_remove_node = head->next;

		if(to_remove_node != tail
				&& CAS_x86(
					(volatile unsigned long long *)&head->next,
					(unsigned long long)to_remove_node,
					(unsigned long long)tail)
					)
		{
			while(to_remove_node != tail)
			{
				tmp = to_remove_node->next;
				if(!is_marked(tmp))
				{
					printf("Found a valid node during prune A @ %u.\n", i);
					printf("Node %.10f, counter %u, index %u\n", to_remove_node->timestamp, to_remove_node->counter, hash(to_remove_node->timestamp, queue->bucket_width));
					error("Found a valid node during prune A.\n");
				}
				tmp = get_unmarked(tmp);
				mm_free(to_remove_node);
				to_remove_node = tmp;
			}
		}
	}

	while(*tmp_previous != NULL)
	{
		to_remove_node = *tmp_previous;
		if(
				//queue == to_remove_node->queue &&
				hash(to_remove_node->timestamp, queue->bucket_width) < end_index
		)
		{
			*tmp_previous = (bucket_node*)(to_remove_node->payload);
			counter = to_remove_node->counter;

			// is a chain of nodes
			while(counter-- != 0)
			{
				tmp = to_remove_node;
				to_remove_node = tmp->next;
				if(!is_marked(to_remove_node) && tmp->to_remove == 0)
					error("Found a valid node during prune B %p %p %p %u %f.\n", tmp, tmp->next, tmp->generator, tmp->to_remove, tmp->timestamp);
				to_remove_node = get_unmarked(to_remove_node);
				mm_free(tmp);
			}
		}
		else
			tmp_previous = (bucket_node**)(&to_remove_node->payload);
	}

	return committed;
}

#pragma GCC diagnostic pop
