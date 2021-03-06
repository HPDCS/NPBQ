#include <stdlib.h>
#include <stdio.h>
//#include <string.h>
//#include <unistd.h>
#include <limits.h>
#include <stdbool.h>
#include <math.h> //mauro
#include <pthread.h>

#include "calqueue.h"

// Declare data structures needed for the schedulers
static calqueue_node *calq[CALQSPACE];	// Array of linked lists of items
static calqueue_node **calendar;	// Pointer to use as a sub-array to calq

// Global variables for the calendar queueing routines
static int firstsub, nbuckets, qsize, lastbucket;
static bool resize_enabled;
static double top_threshold, bot_threshold, lastprio;

static double buckettop, cwidth;

static calqueue_node *calqueue_deq(void);
static void calqueue_enq(calqueue_node *new_node);

static pthread_spinlock_t cal_spinlock;

/* This initializes a bucket array within the array a[].
   Bucket width is set equal to bwidth. Bucket[0] is made
   equal to a[qbase]; and the number of buckets is nbuck.
   Startprio is the priority at which dequeueing begins.
   All external variables except resize_enabled are
   initialized
*/
static void localinit(int qbase, int nbucks, double bwidth, double startprio) {

	int i;
	long int n;

	// Set position and size of nnew queue
	firstsub = qbase;
	calendar = calq + qbase;
	cwidth = bwidth;
	nbuckets = nbucks;

	// Init as empty
	qsize = 0;
	for (i = 0; i < nbuckets; i++) {
		calendar[i] = NULL;
	}

	// Setup initial position in queue
	lastprio = startprio;
	n = (long)((double)startprio / cwidth);	// Virtual bucket
	lastbucket = (int) (n % nbuckets);
	buckettop = (double)(n + 1) * cwidth + 0.5 * cwidth;

	// Setup queue-size-change thresholds
	bot_threshold = (int)((double)nbuckets / 2) - 2;
	top_threshold = 2 * nbuckets;
}

// This function returns the width that the buckets should have
// based on a random sample of the queue so that there will be about 3
// items in each bucket.
static double new_width(void) {

	//printf("calqueue: sto eseguendo new_width\n");

	int nsamples, templastbucket, i, j;
	double templastprio;
	double tempbuckettop, average, newaverage;
	calqueue_node *temp[25];

	// Init the temp node structure
	for (i = 0; i < 25; i++) {
		temp[i] = NULL;
	}

	// How many queue elements to sample?
	if (qsize < 2)
		return 1.0;

	if (qsize <= 5)
		nsamples = qsize;
	else
		nsamples = 5 + (int)((double)qsize / 10);

	if (nsamples > 25)
		nsamples = 25;

	// Store the current situation
	templastbucket = lastbucket;
	templastprio = lastprio;
	tempbuckettop = buckettop;

	resize_enabled = false;
	average = 0.;

	for (i = 0; i < nsamples; i++) {
		// Dequeue nodes to get a test sample and sum up the differences in time
		temp[i] = calqueue_deq();
		if (i > 0)
			average += temp[i]->timestamp - temp[i - 1]->timestamp;
	}

	// Get the average
	average = average / (double)(nsamples - 1);

	newaverage = 0.;
	j = 0;

	// Re-insert temp node 0
	//calqueue_put(temp[0]->timestamp, temp[0]->payload);
	calqueue_enq(temp[0]);

	// Recalculate ignoring large separations
	for (i = 1; i < nsamples; i++) {
		if ((temp[i]->timestamp - temp[i - 1]->timestamp) < (average * 2.0)) {
			newaverage += (temp[i]->timestamp - temp[i - 1]->timestamp);
			j++;
		}
		//calqueue_put(temp[i]->timestamp, temp[i]->payload);
		calqueue_enq(temp[i]);
	}

	// Free the temp structure (the events have been re-injected in the queue)
//	for (i = 0; i < 25; i++) {
//		if (temp[i] != NULL) {
//			free(temp[i]);
//		}
//	}

	// Compute new width
	newaverage = (newaverage / (double)j) * 3.0;	/* this is the new width */

	// Restore the original condition
	lastbucket = templastbucket;	/* restore the original conditions */
	lastprio = templastprio;
	buckettop = tempbuckettop;
	resize_enabled = true;

	return newaverage;
}

// This copies the queue onto a calendar with nnewsize buckets. The new bucket
// array is on the opposite end of the array a[QSPACE] from the original        EH?!?!?!?!?!
static void resize(int newsize) {
	double bwidth;
	int i;
	int oldnbuckets;
	calqueue_node **oldcalendar, *temp, *temp2;

	if (!resize_enabled)
		return;

	//printf("calqueue: sto eseguendo resize\n");

	// Find new bucket width
	bwidth = new_width();

	// Save location and size of old calendar for use when copying calendar
	oldcalendar = calendar;
	oldnbuckets = nbuckets;

	// Init the new calendar
	if (firstsub == 0) {
		localinit(CALQSPACE - newsize, newsize, bwidth, lastprio);
	} else {
		localinit(0, newsize, bwidth, lastprio);
	}

	// Copy elements to the new calendar
	for (i = oldnbuckets - 1; i >= 0; --i) {
		temp = oldcalendar[i];
		while (temp != NULL) {
			temp2 = temp->next;
			calqueue_enq(temp);
			temp = temp2;
		}
//		while (temp != NULL) {
//			calqueue_put(temp->timestamp, temp->payload);
//			temp2 = temp->next;
//			free(temp);
//			temp = temp2;
//		}
	}
}

static calqueue_node *calqueue_deq(void) {

	register int i;
	int temp2;
	calqueue_node *e;
	double lowest;

	// Is there an event to be processed?
	if (qsize == 0) {
		return NULL;
	}

	for (i = lastbucket;;) {
		// Check bucket i
		if (calendar[i] != NULL && calendar[i]->timestamp < buckettop) {

 calendar_process:

			// Found an item to be processed
			e = calendar[i];

			// Update position on calendar and queue's size
			lastbucket = i;
			lastprio = e->timestamp;


			qsize--;

			// Remove the event from the calendar queue
			calendar[i] = calendar[i]->next;

			// Halve the calendar size if needed
			if (qsize < bot_threshold)
				resize((int)((double)nbuckets / 2));

			//if(lastprio>248 && lastprio<251) printf("\tget: lastbucket %d, timestamo_head %f, buckettop %f\n", i, calendar[i]->timestamp, buckettop);

			// Processing completed
			return e;

		} else {
			// Prepare to check the next bucket, or go to a direct search
			i++;
			if (i == nbuckets)
				i = 0;

			buckettop += cwidth;

			if (i == lastbucket)
				break;	// Go to direct search
		}
	}

	// Directly search for minimum priority event
	temp2 = -1;
	lowest = (double)LLONG_MAX;
	for (i = 0; i < nbuckets; i++) {
		if ((calendar[i] != NULL) && ((temp2 == -1) || (calendar[i]->timestamp < lowest))) {
			temp2 = i;
			lowest = calendar[i]->timestamp;
		}
	}

	// Process the event found and and handle the queue
	i = temp2;
	goto calendar_process;
	return NULL;

}

static void calqueue_enq(calqueue_node *new_node) {

	int i, i_bt;
	calqueue_node *traverse;
	double timestamp = new_node->timestamp;

	//printf("calqueue: inserendo %f, cwidth %f, bukettop %f, nbuckets %d, lastprio %f\n", timestamp, cwidth, buckettop, nbuckets, lastprio);

	if(new_node == NULL){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();
	}

	// Calculate the number of the bucket in which to place the new entry
	i_bt = (int) (round(timestamp / (double)cwidth));	// Virtual bucket
	i = i_bt % nbuckets;	// Actual bucket

	// Grab the head of events list in bucket i
	traverse = calendar[i];

	// Put at the head of the list, if appropriate
	if (traverse == NULL || traverse->timestamp > timestamp) {
		new_node->next = traverse;
		calendar[i] = new_node;
	} else {
		// Find the correct place in list
		while (traverse->next != NULL && traverse->next->timestamp <= timestamp)
			traverse = traverse->next;

		// Place the new event
		new_node->next = traverse->next;
		traverse->next = new_node;
	}

	// Update queue size
	qsize++;

	// Check whether we're adding something before lastprio
	if (timestamp < lastprio) {
		lastprio = timestamp;
		lastbucket = i;
		buckettop = (double)(i_bt + 1) * cwidth + 0.5 * cwidth;//
	}

	// Double the calendar size if needed
	if (qsize > top_threshold && nbuckets < MAXNBUCKETS) {
		resize(2 * nbuckets);
	}

	//if(timestamp>248 && timestamp<251) printf("\tput: lastbucket %d, timestamo_head %f, buckettop %f\n", i, calendar[i]->timestamp, buckettop);
}

// This function initializes the messaging queue.
void calqueue_init(void) {

	localinit(0, 2, 1.0, 0.0);
	resize_enabled = true;
	pthread_spin_init(&cal_spinlock, 0);
}

void calqueue_put(double timestamp, void *payload) {

	calqueue_node *new_node;

	//printf("calqueue: inserendo %f, cwidth %f, bukettop %f, nbuckets %d, lastprio %f\n", timestamp, cwidth, buckettop, nbuckets, lastprio);

	// Fill the node entry
	new_node = malloc(sizeof(calqueue_node));
	new_node->timestamp = timestamp;
	new_node->payload = payload;
	new_node->next = NULL;

	if(new_node == NULL){
		printf("Out of memory in %s:%d\n", __FILE__, __LINE__);
		abort();
	}

	pthread_spin_lock(&cal_spinlock);
	calqueue_enq(new_node);
	pthread_spin_unlock(&cal_spinlock);

}

calqueue_node *calqueue_get(void) {
	calqueue_node *node;

	pthread_spin_lock(&cal_spinlock);
	node = calqueue_deq();
	pthread_spin_unlock(&cal_spinlock);
	if (node == NULL) {
		return NULL;
	}

	//printf("calqueue: estraendo %f, cwidth %f, bukettop %f, nbuckets %d, lastprio %f\n", node->timestamp, cwidth, buckettop, nbuckets, lastprio);

	return node;
}
