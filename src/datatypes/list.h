/**
*			Copyright (C) 2008-2015 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file list.h
* @brief This header defines macros for accessing the general-purpose list
* 	 implementation used in the simulator
* @author Alessandro Pellegrini
* @date November 5, 2013
*/

#pragma once
#ifndef __LIST_DATATYPE_H
#define __LIST_DATATYPE_H

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "../mm/myallocator.h"


//#include <core/core.h>
//#include <mm/malloc.h>

//#include <arch/atomic.h>

/// This is the encapsulating structure of a list node. Any payload can be contained by this.
struct rootsim_list_node {
	struct rootsim_list_node *next;
	struct rootsim_list_node *prev;
	char data[];
};


typedef struct rootsim_list rootsim_list;
/// This structure defines a generic list.
struct rootsim_list {
	unsigned int size;
	pthread_spinlock_t  spinlock;
	struct rootsim_list_node *head;
	struct rootsim_list_node *tail;
	//atomic_t counter;
};


/// This macro is a slightly-different implementation of the standard offsetof macro
#define my_offsetof(st, m) ((size_t)( (unsigned char *)&((st)->m ) - (unsigned char *)(st)))

/// Declare a "typed" list. This is a pointer to type, but the variable will intead reference a struct rootsim_list!!!
#define list(type) type *

/** This macro allocates a struct rootsim_list object and cast it to the type pointer.
 *  It can be used to mimic the C++ syntax of templated lists, like:
 *  \code
 *   list(int) = new_list(int);
 *  \endcode
 */
#define new_list(type)	(type *)({ \
				void *__lmptr = (void *)rsalloc(sizeof(struct rootsim_list));\
				bzero(__lmptr, sizeof(struct rootsim_list));\
				__lmptr;\
			})

/// Insert a new node in the list. Refer to <__list_insert_head>() for a more thorough documentation.
#define list_insert_head(list, data) \
			(__typeof__(list))__list_insert_head((list), sizeof *(list), (data))


/// Insert a new node in the list. Refer to <__list_insert_tail>() for a more thorough documentation.
#define list_insert_tail(list, data) \
			(__typeof__(list))__list_insert_tail((list), sizeof *(list), (data))


/// Insert a new node in the list. Refer to <__list_insert>() for a more thorough documentation.
#define list_insert(list, key_name, data) \
			(__typeof__(list))__list_insert((list), sizeof *(list), my_offsetof((list), key_name), (data))

/// Remove a node in the list. Refer to <__list_delete>() for a more thorough documentation.
#define list_delete(list, key_name, key_value) \
		__list_delete((list), sizeof *(list), (double)(key_value), my_offsetof((list), key_name))

/// Remove a node in the list and returns its content. Refer to <__list_extract>() for a more thorough documentation.
#define list_extract(list, key_name, key_value) \
		(__typeof__(list))__list_extract((list), sizeof *(list), (double)(key_value), my_offsetof((list), key_name))

/** Remove a node in the list and returns its content by the pointer of the data contained in the node.
 *  Refer to <__list_extract_by_content>() for a more thorough documentation.
 */
#define list_extract_by_content(list, ptr) \
		(__typeof__(list))__list_extract_by_content((list), sizeof *(list), (ptr), true)

/** Remove a node in the list by the pointer of the data contained in the node
 *  Refer to <__list_delete_by_content>() for a more thorough documentation.
 *  TODO: there is a memory leak here
 */
#define list_delete_by_content(list, ptr) \
		(void)__list_extract_by_content((list), sizeof *(list), (ptr), false)

/// Find a node in the list. Refer to <__list_find>() for a more thorough documentation.
#define list_find(list, key_name, key_value) \
		(__typeof__(list))__list_find((list), (double)(key_value), my_offsetof((list), key_name))


#define LIST_TRUNC_AFTER	10
#define LIST_TRUNC_BEFORE	11

/// Truncate a list up to a certain point, towards increasing values. Refer to <__list_trunc>() for a more thorough documentation.
#define list_trunc_before(list, key_name, key_value) \
		__list_trunc((list), (double)(key_value), my_offsetof((list), key_name), LIST_TRUNC_BEFORE)

/// Truncate a list from a certain point, towards increasing values. Refer to <__list_trunc>() for a more thorough documentation.
#define list_trunc_after(list, key_name, key_value) \
		__list_trunc((list), (double)(key_value), my_offsetof((list), key_name), LIST_TRUNC_AFTER)


// Get the size of the current list. Refer to <__list_delete>() for a more thorough documentation.
#define list_sizeof(list) ((struct rootsim_list *)list)->size


/**
 * This macro retrieves a pointer to the payload of the head node of a list.
 *
 * @param list a pointer to a list created using the <new_list>() macro.
 */
#define list_head(list) ({\
			struct rootsim_list_node *__headptr = ((struct rootsim_list *)(list))->head;\
			__typeof__(list) __dataptr = (__typeof__(list))(__headptr == NULL ? NULL : __headptr->data);\
			__dataptr;\
			})


/**
 * This macro retrieves a pointer to the payload of the head node of a list.
 *
 * @param list a pointer to a list created using the <new_list>() macro.
 */
#define node_payload(list, node) ({\
			struct rootsim_list_node *__headptr = ((struct rootsim_list_node *)node);\
			__typeof__(list) __dataptr = (__typeof__(list))(__headptr == NULL ? NULL : __headptr->data);\
			__dataptr;\
			})


/**
 * This macro retrieves a pointer to the payload of the tail node of a list.
 *
 * @param list a pointer to a list created using the <new_list>() macro.
 */
#define list_tail(list) ({\
			struct rootsim_list_node *__tailptr = ((struct rootsim_list *)(list))->tail;\
			__typeof__(list) __dataptr = (__typeof__(list))(__tailptr == NULL ? NULL : __tailptr->data);\
			__dataptr;\
			})


/**
 * This macro checks whether a given pointer corresponds to the list head's payload.
 *
 * @param list a pointer to a list created using the <new_list>() macro.
 */
#define list_is_head(list, ptr) ({\
				bool __isheadbool = (((struct rootsim_list*)(list))->head == list_container_of((ptr)));\
				if(ptr == NULL) {\
					__isheadbool = false;\
				}\
				__isheadbool;\
				})


/**
 * This macro checks whether a given pointer corresponds to the list tail's payload.
 *
 * @param list a pointer to a list created using the <new_list>() macro.
 */
#define list_is_tail(list, ptr) ({\
				bool __istailbool = (((struct rootsim_list *)(list))->tail == list_container_of((ptr)));\
				if(ptr == NULL) {\
					__istailbool = false;\
				}\
				__istailbool;\
				})


/**
 * Given a pointer to a list node's payload, this macro retrieves the next node's payload, if any.
 *
 * @param list a pointer to a list created using the <new_list>() macro.
 */
#define list_next(ptr) ({\
			struct rootsim_list_node *__nextptr = list_container_of(ptr)->next;\
			__typeof__(ptr) __dataptr = (__typeof__(ptr))(__nextptr == NULL ? NULL : __nextptr->data);\
			__dataptr;\
			})


/**
 * Given a pointer to a list node's payload, this macro retrieves the prev node's payload, if any.
 *
 * @param list a pointer to a list created using the <new_list>() macro.
 */
#define list_prev(ptr) ({\
			struct rootsim_list_node *__prevptr = list_container_of(ptr)->prev;\
			__typeof__(ptr)__dataptr = (__typeof__(ptr))(__prevptr == NULL ? NULL : __prevptr->data);\
			__dataptr;\
			})








/// This macro allows to get a pointer to the struct rootsim_list_node containing the passed ptr
#define list_container_of(ptr) ((struct rootsim_list_node *)( (char *)(ptr) - offsetof(struct rootsim_list_node, data) ))

/// This macro retrieves the key of a payload data structure given its offset, and casts the value to double.
#define get_key(data) ({\
			char *__key_ptr = ((char *)(data) + key_position);\
			double *__key_double_ptr = (double *)__key_ptr;\
			*__key_double_ptr;\
		      })

#define list_empty(list) (((rootsim_list *)list)->size == 0)

extern char *__list_insert_head(void *li, unsigned int size, void *data);
extern char *__list_insert_tail(void *li, unsigned int size, void *data);
extern char *__list_insert(void *li, unsigned int size, size_t key_position, void *data);
extern char *__list_extract(void *li, unsigned int size, double key, size_t key_position);
extern bool __list_delete(void *li, unsigned int size, double key, size_t key_position);
extern char *__list_extract_by_content(void *li, unsigned int size, void *ptr, bool copy);
extern char *__list_find(void *li, double key, size_t key_position);
extern unsigned int __list_trunc(void *li, double key, size_t key_position, unsigned short int direction);
extern void* list_pop(void *li);

#endif /* __LIST_DATATYPE_H */

