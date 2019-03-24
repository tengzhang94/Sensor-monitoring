#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <time.h>
#include "sbuffer.h"
#include "sensor_db.h"

/*when there is no data in the sbuffer readers 
 *wait a while before it closes
 */
#define cond_wait_time 7 

/*
 * All data that can be stored in the sbuffer should be encapsulated in a
 * structure, this structure can then also hold extra info needed for your implementation
 */
struct sbuffer_data {
    sensor_data_t data;
};

typedef struct sbuffer_node {
  struct sbuffer_node * next;
  sbuffer_data_t element;
} sbuffer_node_t;

struct sbuffer {
  sbuffer_node_t * head;
  sbuffer_node_t * tail;
	pthread_mutex_t mutex_remove_sbuffer_node;	
	pthread_barrier_t barrier_data_read;		
	pthread_barrier_t barrier_data_removed;

	pthread_mutex_t sbuffer_empty_lock;
	pthread_cond_t sbuffer_not_empty;
};	


int sbuffer_init(sbuffer_t ** buffer)
{
  *buffer = malloc(sizeof(sbuffer_t));
  if (*buffer == NULL) return SBUFFER_FAILURE;
  (*buffer)->head = NULL;
  (*buffer)->tail = NULL;
	pthread_cond_init(&((*buffer)->sbuffer_not_empty),NULL);
	pthread_mutex_init(&((*buffer)->mutex_remove_sbuffer_node),NULL);
	/** this combination is for cond_t to make sure sbuffer is not empty
	before it is read
	**/ 
	pthread_mutex_init(&((*buffer)->sbuffer_empty_lock),NULL);
	//initialize barrier
    int rc = pthread_barrier_init(&((*buffer)->barrier_data_read), NULL, 2);
	int rc1 = pthread_barrier_init(&((*buffer)->barrier_data_removed), NULL, 2);
	if (rc || rc1) 
	{
		DEBUG_PRINTF("pthread_barrier_init failed\n");
        exit(1);
	}
  return SBUFFER_SUCCESS; 
}


int sbuffer_free(sbuffer_t ** buffer)
{
  if ((buffer==NULL) || (*buffer==NULL)) 
  {
    return SBUFFER_FAILURE;
  } 
	pthread_barrier_destroy(&((*buffer)->barrier_data_removed));
	pthread_barrier_destroy(&((*buffer)->barrier_data_read));
	pthread_mutex_destroy(&((*buffer)->mutex_remove_sbuffer_node));
	pthread_mutex_destroy(&((*buffer)->sbuffer_empty_lock));
	pthread_cond_destroy(&((*buffer)->sbuffer_not_empty));
  while ( (*buffer)->head )
  {
	sbuffer_node_t * dummy;
    dummy = (*buffer)->head;
    (*buffer)->head = (*buffer)->head->next;
    free(dummy);
  }
  free(*buffer);
  *buffer = NULL;
  return SBUFFER_SUCCESS;		
}


int sbuffer_remove(sbuffer_t * buffer,sensor_data_t * data) //remove from the head
{
  int the_thread_removes_data = FALSE ;
  sbuffer_t * sbuffer = buffer;
  if (sbuffer == NULL) return SBUFFER_FAILURE;
  /* when sbuffer is empty reader threads block here */
  pthread_mutex_lock(&(buffer->sbuffer_empty_lock));
  while (sbuffer->head == NULL)  //sbuffer is empty
  {
	/*** wait till a new node is added and
	   a broadcast signal will also be received
	   set a time so it does not wait forever(the program will block forever) ***/
	struct timespec ts;	
	clock_gettime(CLOCK_REALTIME, &ts);
    	ts.tv_sec += cond_wait_time;
	if(pthread_cond_timedwait(&(buffer->sbuffer_not_empty), &(buffer->sbuffer_empty_lock), &ts) == ETIMEDOUT)
	{
		pthread_mutex_unlock(&(buffer->sbuffer_empty_lock));
  		return SBUFFER_NO_DATA;
	} 
  }
  pthread_mutex_unlock(&(buffer->sbuffer_empty_lock));

  /** now we have made sure sbuffer is not empty **/
  sensor_data_t * data_ptr = data;
  /*acquiring data is done here*/
  (*data_ptr) = sbuffer->head->element.data;
  OBSERVE_PRINTF(" this one is read :sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", sbuffer->head->element.data.id, sbuffer->head->element.data.value, (long int)(sbuffer->head->element.data.ts));
  /* wait till both have read data */
  pthread_barrier_wait(&(buffer->barrier_data_read));
  /*removing data from sbuffer happens here */
  if(pthread_mutex_trylock (&(buffer->mutex_remove_sbuffer_node)) == 0)
  {
  the_thread_removes_data = TRUE ;
  sbuffer_node_t * dummy = sbuffer->head;
  if (sbuffer->head == sbuffer->tail) // buffer has only one node
  {
    sbuffer->head = sbuffer->tail = NULL; 
  }
  else  // buffer has many nodes empty
  {
    sbuffer->head = sbuffer->head->next;
  }
  free(dummy);
  OBSERVE_PRINTF("One data is removed\n");
  }
	/*synchronize here again to make sure both threads have done try_lock before we unlock it */
	pthread_barrier_wait(&(buffer->barrier_data_removed));
	/*now we can unlock and both threads go to next iteration */
	if(the_thread_removes_data == TRUE)  pthread_mutex_unlock(&(buffer->mutex_remove_sbuffer_node));
	else OBSERVE_PRINTF("Remove is done by another thread\n");
	return SBUFFER_SUCCESS;
}


int sbuffer_insert(sbuffer_t * buffer, sensor_data_t * data) //insert at the tail
{
  sbuffer_node_t * dummy;
  if (buffer == NULL) return SBUFFER_FAILURE;
  dummy = malloc(sizeof(sbuffer_node_t));
  if (dummy == NULL) return SBUFFER_FAILURE;
  dummy->element.data = *data;
  dummy->next = NULL;
  if (buffer->tail == NULL && buffer->head == NULL) // buffer empty (buffer->head should also be NULL
  {
	//lock before add a new node
	pthread_mutex_lock(&(buffer->sbuffer_empty_lock));
    buffer->head = buffer->tail = dummy;
    OBSERVE_PRINTF(" Insert() :sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", buffer->tail->element.data.id, buffer->tail->element.data.value, (long int)(buffer->tail->element.data.ts));
	//a new node added, reactivate reader threads
	pthread_cond_broadcast(&(buffer->sbuffer_not_empty));
	OBSERVE_PRINTF(" signal sent\n");
	//unlock mutex so pthread_cond_wait can gain the mutex
	pthread_mutex_unlock(&(buffer->sbuffer_empty_lock));
  } 
  else // buffer not empty
  {
    buffer->tail->next = dummy;
    buffer->tail = buffer->tail->next; 
	OBSERVE_PRINTF(" Insert() :sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", buffer->tail->element.data.id, buffer->tail->element.data.value, (long int)(buffer->tail->element.data.ts));
  }
  return SBUFFER_SUCCESS;
}

void sbuffer_print(sbuffer_t * buffer1)
{
	sbuffer_t * buffer = buffer1;
	sbuffer_node_t * dummy;
	dummy = (sbuffer_node_t *) (buffer->head);
	OBSERVE_PRINTF("Those still left in sbuffer are: \n");
	do
	{
		if(dummy == NULL) break; //no data yet 
		OBSERVE_PRINTF("Sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", dummy->element.data.id, dummy->element.data.value, (long int)dummy->element.data.ts);
		if(dummy->next == NULL) break;
		dummy = dummy->next;
	}while(1);
	OBSERVE_PRINTF("sbuffer_print finished\n");

}

