#ifndef _CONFIG_H_
#define _CONFIG_H_
#define     _GNU_SOURCE

#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <assert.h>

#define TRUE 1
#define FALSE 0

#define RECV_BUFF_MAX 200
#ifndef FIFO
  #define FIFO "logFifo"
#endif

/** Error message macro **/
#define ERROR_MEMORY  "malloc failed"
#define ERROR_FORK    "fork failed to create a child"
#define ERROR_THREAD_CREATE "threads creation failed"
#define ERROR_OPEN    "open file failed"
#define DPL_OP_FAIL    "dpl operation failed"

/** MACRO's used in multiple files **/
#define WRITE_FIFO(...)										\
	do { 											\
		pthread_mutex_lock(&mutex_fifo);  			\
		int fifo = open(FIFO,O_WRONLY | O_APPEND);			\
		ERR_HANDLER(fifo == -1, ERROR_OPEN);        \
		char write_buffer[RECV_BUFF_MAX];					\
		sprintf(write_buffer,__VA_ARGS__);					\
		write(fifo,write_buffer,strlen(write_buffer));		\
		pthread_mutex_unlock(&mutex_fifo);  				\
		close(fifo);		 							\
	} while(0)

#ifdef DEBUG
	#define DEBUG_PRINTF(...) 									         \
		do {											         \
			fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	 \
			fprintf(stderr,__VA_ARGS__);								 \
			fflush(stderr);                                                                          \
                } while(0)
	#define DEBUG_TIP(void) (void)0
#else
	#define DEBUG_PRINTF(...) (void)0
	#define DEBUG_TIP(void) printf("Use -DDEBUG=1 to compile if you want to see debug info\n");
#endif

#ifdef OBSERVE
	#define OBSERVE_PRINTF(...) 			  \
		do {								  \
			printf(__VA_ARGS__);                        \
			} while (0)
	#define OBSERVE_TIP(void) (void)0
#else
	#define OBSERVE_PRINTF(...) (void)0
	#define OBSERVE_TIP(void) printf("Use -DOBSERVE=1 to compile if you want to see progress printed out\n");
#endif

#define ERR_HANDLER(condition,err_code)\
	do {						            \
            if ((condition)) DEBUG_PRINTF(err_code);    \
            assert(!(condition));                                    \
        } while(0)

typedef uint16_t sensor_id_t;
typedef double sensor_value_t;     
typedef time_t sensor_ts_t;         // UTC timestamp as returned by time() - notice that the size of time_t is different on 32/64 bit machine

typedef struct{
	sensor_id_t id;
	sensor_value_t value;
	sensor_ts_t ts;
} sensor_data_t;
int forTest;

pthread_mutex_t mutex_fifo;
#endif /* _CONFIG_H_ */

