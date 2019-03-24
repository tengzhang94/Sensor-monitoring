#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <signal.h>
#include <sys/wait.h>
#include "lib/tcpsock.h"
#include "connmgr.h"
#include "sbuffer.h"
#include "sensor_db.h"
#include "datamgr.h"

void logger(void);
void cleanup_log(int sig);
void * writer_connmgr(void * args);
void * reader_storagemgr(void* args);
void * reader_datamgr(void * args);

/** to print out everything left in sbuffer **/
void  sbuffer_print(sbuffer_t * buffer1);
/** print out sensors found in DB **/
int callback(void *NotUsed, int argc, char **argv,  char **azColName);

/**input struct for connmgr's thread **/
typedef struct connmgr_args{
	int nr_port;
	sbuffer_t ** buffer;
} connmgr_args_t ;

/**input struct for datamgr's thread **/
typedef struct datamgr_args{
	FILE * fp_sensor_map;
	sbuffer_t ** buffer;
} datamgr_args_t ;

sbuffer_t * sbuffer;
DBCONN * con;
int fp_fifo_rd;
FILE * fp_gateway;
/** mutex for fifo operations**/
pthread_mutex_t  mutex_fifo = PTHREAD_MUTEX_INITIALIZER; 

int main(int argc, char* argv[])
{	forTest = 345;
	printf("HERE WE CAN SEE IF THE VARIABLE FORTEST IS ACCESSIBLE IN MAIN: %d\n",forTest);
	int PORT;
	if (argc != 2) 
	{
		printf("Please input an arguement as the port number !!\n");
		exit(EXIT_FAILURE);
	}
  	else PORT = atoi(argv[1]);
	OBSERVE_TIP(void);
	DEBUG_TIP(void);
	int result = mkfifo(FIFO,0666);
	if (result == -1) DEBUG_PRINTF("FIFO existed already\n");
	else DEBUG_PRINTF("Created a Fifo \n");	
	pid_t pid;
	pid = fork();
	ERR_HANDLER(pid < 0, ERROR_FORK);
	if(pid == 0) // create log process as a child
	{
		/**asynchronously catching a signal **/
		signal(SIGUSR1, cleanup_log);
		logger();		
	}  	
	FILE * fp_sensor_map = fopen("room_sensor.map","r");
	con = init_connection(1);

	// initialize sbuffer
	if(sbuffer_init(&sbuffer) != SBUFFER_SUCCESS) exit(EXIT_FAILURE);
	// create a struct as input for thread_writer
	connmgr_args_t writer_args;
	writer_args.nr_port = PORT;
	writer_args.buffer =(sbuffer_t **) &sbuffer;
	// create a struct as input for thread_datamgr
	datamgr_args_t reader_datamgr_args;
	reader_datamgr_args.fp_sensor_map = fp_sensor_map;
	reader_datamgr_args.buffer = (sbuffer_t **) &sbuffer;

	pthread_t thread_writer;
	pthread_t thread_reader_storagemgr;
	pthread_t thread_reader_datamgr;
	// for check-up of pthreads status
	int ret[3] ={-1, -1, -1};
	// create threads
	ret[0] = pthread_create(&thread_writer, NULL, writer_connmgr, (void*)&writer_args);
	ret[1] = pthread_create(&thread_reader_storagemgr, NULL, reader_storagemgr,(void*) sbuffer);
	ret[2] = pthread_create(&thread_reader_datamgr, NULL, reader_datamgr,(void*)&reader_datamgr_args);
	for(int i = 0; i<3; i++)
	{
	 ERR_HANDLER(ret[i] != 0, ERROR_THREAD_CREATE);
	}

	// join() to wait till threads terminates
	pthread_join( thread_writer, NULL);	
	pthread_join( thread_reader_storagemgr, NULL);	
	pthread_join( thread_reader_datamgr, NULL);

	// to see what is left in sbuffer and free sbuffer
	// only prints in OBSERVE mode
	sbuffer_print(sbuffer);
	if(sbuffer_free(&sbuffer) != SBUFFER_SUCCESS) DEBUG_PRINTF("sbuffer_free failed\n");

	do
	{
		kill(pid, SIGUSR1);
		OBSERVE_PRINTF("child is killed\n");
	}while (wait(NULL) != pid); //if child is not terminated so we kill again
	return 0;
}

/*** functions for threads and logger function ***/
void logger(void)
{
	int sequence_nr = 0;
	fp_gateway = fopen("gateway.log", "w");
	char buffer_rd[RECV_BUFF_MAX];
	memset(buffer_rd,0,RECV_BUFF_MAX);	
	while(1)
	{
        fp_fifo_rd = open(FIFO, O_RDONLY);
		ERR_HANDLER(fp_fifo_rd == -1, ERROR_OPEN);
		if(read(fp_fifo_rd,buffer_rd, RECV_BUFF_MAX)>0)
		{
		fprintf(fp_gateway, "%d %ld %s", sequence_nr++, (long int)time(NULL), buffer_rd);	
		fflush(fp_gateway );
		memset(buffer_rd,0,RECV_BUFF_MAX);
		}	
	}
}

void * writer_connmgr(void * args)
{
	int nr_port = ((connmgr_args_t*)args)->nr_port;
	sbuffer_t** buffer = ((connmgr_args_t*)args)->buffer;
	connmgr_listen(nr_port, buffer);
  	connmgr_free();
 	return NULL;
}

void * reader_storagemgr(void * args)
{
	sbuffer_t ** buffer = (sbuffer_t **)&args;
	OBSERVE_PRINTF("storagemgr started\n");	
	storagemgr_parse_sensor_data(con, buffer);
	// print all stored in DB
	find_sensor_all(con, callback);
	disconnect(con);
	return NULL;
	 
}

void * reader_datamgr(void *args)
{
	OBSERVE_PRINTF("datamgr started\n");
	FILE * fp_sensor_map_temp = ((datamgr_args_t *)args)->fp_sensor_map ;
	sbuffer_t ** buffer_temp = ((datamgr_args_t *)args)->buffer;
	datamgr_parse_sensor_data(fp_sensor_map_temp, buffer_temp);
	datamgr_free();
	fclose(fp_sensor_map_temp);
	return NULL;
}

/*** call back to print out DB data ***/
int callback(void *NotUsed, int argc, char **argv,  char **azColName) 
{    
    NotUsed = 0;
    OBSERVE_PRINTF("Those in DB are:\n");
    for (int i = 0; i < argc; i++) 
	{
        OBSERVE_PRINTF("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    OBSERVE_PRINTF("\n");
    return 0;
}

void cleanup_log(int sig)
{
	fclose(fp_gateway);		//close the log file
	close(fp_fifo_rd) ; 	//close the fifo
	remove(FIFO);			//remove the fifo before exiting
	OBSERVE_PRINTF("Cleanup_log succesfully finished\n");	
	exit(0);
}

