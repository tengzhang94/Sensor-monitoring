#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "lib/tcpsock.h"
#include "connmgr.h"
#include "lib/dplist.h"
#include "sbuffer.h"

#ifndef TIMEOUT
#error "TIMEOUT is not set"
#endif

#define TRUE 1
#define FALSE 0

//global variables
dplist_t * sockets;
struct sock{
		tcpsock_t* socket;
		time_t last_ts;
		sensor_id_t sensor_id;
		int first_receive;
};

//call back functions are below
void* element_copy(void * src_element)
{
  	struct sock* temp_sock= malloc(sizeof(struct sock));
	ERR_HANDLER(temp_sock== NULL, ERROR_MEMORY);
  	*temp_sock=*(struct sock*)src_element;
  	return temp_sock;
}

void element_free(void ** element)
{
 	free(*element);
}
int element_compare(void * x, void * y) //convert to int before comparison
{
	if(*(int*)x>*(int*)y)
      	return 1;
    	if(*(int*)x==*(int*)y)
      	return 0;
    	else
     	return -1;
}

//connmgr_listen to listen to request of connections
void connmgr_listen(int port_nr, sbuffer_t ** buffer)
{
	printf("HERE WE CAN SEE IF THE VARIABLE FORTEST IS ACCESSIBLE IN CONNMGR: %d\n", forTest);
	sbuffer_t * sbuffer = (sbuffer_t *)*buffer;
	int end_server = FALSE;
	int nr_time_out = 0;

	sensor_data_t data;
	tcpsock_t * server, * client;
	int port = port_nr;
	int nfds;
	int server_sd = -1, client_sd = -1;
	//time_t  current;
	struct sock socket_with_ts;

	//open server
	if (tcp_passive_open(&server,port)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
	OBSERVE_PRINTF("Server is created\n");
	//get server descriptor
	if(tcp_get_sd(server,&server_sd)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
	//create dplist to manage connections
	sockets= dpl_create(&element_copy,&element_free,&element_compare);
	nfds = dpl_size(sockets)+1;
	  
	//part of the use of poll() here comes from a modified version of code I read and understood, from www.ibm.com
	
	do
	{
			// every iteration initialize a new fds[]
			// thus the nr of sensor will not be limited
			// the "+1" is for potential new connection
			// but the the poll does not apply to the "+1"
			struct pollfd fds[nfds+1];
			memset(fds, 0, sizeof(fds)); 
			//put server at the 1st of fds	
			fds[0].fd = server_sd;	     
			fds[0].events = POLLIN;

			for (int k =0; k<dpl_size(sockets); k++)
			{
				struct sock * temp_client = (struct sock *)dpl_get_element_at_index(sockets, k);
				ERR_HANDLER(temp_client == NULL, DPL_OP_FAIL);
				int temp_sd;
				if(tcp_get_sd(temp_client->socket, &temp_sd)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
				//set every connection'event as POLLIN
				fds[k+1].fd = temp_sd;
          		fds[k+1].events = POLLIN;
			}

			int rc = poll(fds, nfds,TIMEOUT*1000); //unit of the 3rd input is mSec
		    //check the last_ts of every socket_with_ts in sockets
			int index_time_out_socket = -2;
			int flag_time_out_socket = FALSE;
			for(int j=0; j<dpl_size(sockets); j++)
			{
				struct sock * temp_client = (struct sock *)dpl_get_element_at_index(sockets, j);
				ERR_HANDLER(temp_client == NULL, DPL_OP_FAIL);
			    time_t ts = temp_client->last_ts;	
				//time_t now = time(&current);
				time_t now =time(NULL);				
				//printf("time_now is %ld, time_then is %ld difftime = %ld\n", (long int)now, (long int)ts, (long int)difftime(ts,now));
				if(difftime(now,ts)>TIMEOUT)
				{
					//we leave the removal for later so we dont mess up--
					// --the order of sockets because there might be--
					// -- other incoming data which needs the socket to --
					// -- stay intact
					 index_time_out_socket = j;
					 flag_time_out_socket = TRUE;
					 OBSERVE_PRINTF("One timed-out connection is found \n");
				}		
			}
		
		if (rc < 0)
    		{
      			perror("Poll() failed");
      			break;
    		}
		if (rc == 0 && nr_time_out == 0) 
			{
				nr_time_out++;
				OBSERVE_PRINTF("Time out happens once\n");
				continue;
			}
		if (rc == 0 && nr_time_out == 1) //already timed out once before
    		{
				nr_time_out++;
      			OBSERVE_PRINTF("Poll() timed out twice in a row. End program.\n");
      			break;
    		}
		// more than one I/O event happened then we look for it
		for(int i = 0;i < dpl_size(sockets)+1; i++)
		{	
			//I/O event did not happen on this one
			if(fds[i].revents == 0)  
			continue;		
			if(fds[i].revents != POLLIN)
      		{
        			OBSERVE_PRINTF("Error! revents = %d\n", fds[i].revents);
        			end_server = TRUE;
        			break;
			}
			//if it is the server socket with revents==POLLIN
			//serve has heard new request
			if(fds[i].revents == POLLIN && fds[i].fd == server_sd)
			{
					OBSERVE_PRINTF("Server is readable\n");
				
					//To make new connections with an incoming request
					if(tcp_wait_for_connection(server, &client)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
					if(tcp_get_sd(client, &client_sd)!=TCP_NO_ERROR) exit(EXIT_FAILURE);
					OBSERVE_PRINTF("New connection!\n");
					
					//everytime there is activity it sets to 0
					nr_time_out = 0; 
					//set every connection'event as POLLIN
					fds[nfds].fd = client_sd;
          fds[nfds].events = POLLIN;
          			
					nfds++;
					OBSERVE_PRINTF("nfds=%d\n",nfds);
					//add new connection to dplist
					socket_with_ts.socket = client;
					//socket_with_ts.last_ts = time(&current);
					socket_with_ts.last_ts = time(NULL);
					socket_with_ts.sensor_id = -1 ;
					socket_with_ts.first_receive = FALSE;				
					dpl_insert_at_index(sockets, &socket_with_ts,dpl_size(sockets),true); //true == element_copy
				 
			}
			// if it is an existing connection, we read data
			else
			{
				if(index_time_out_socket == i-1) continue;
				struct sock* client_sock = (struct sock *)dpl_get_element_at_index(sockets, i-1);
				ERR_HANDLER(client_sock == NULL, DPL_OP_FAIL);
				client = client_sock-> socket;
				int bytes, result;
				    // read sensor ID
      				bytes = sizeof(data.id);
      				result = tcp_receive(client,(void *)&data.id,&bytes);
      				// read temperature
      				bytes = sizeof(data.value);
      				result = tcp_receive(client,(void *)&data.value,&bytes);
      				// read timestamp
      				bytes = sizeof(data.ts);
      				result = tcp_receive( client, (void *)&data.ts,&bytes);
					
					if ((result==TCP_NO_ERROR) && bytes) 
      				{	//every time there is activity it returns to 0
						nr_time_out = 0;
        				
						//update timestamp last_ts
						client_sock->last_ts = data.ts;
						client_sock->sensor_id = data.id;
						
						if(client_sock->first_receive == FALSE)
						{
						
						OBSERVE_PRINTF("A sensor node with %"PRIu16" has opened a new connection\n",client_sock->sensor_id);
						WRITE_FIFO("A sensor node with %"PRIu16" has opened a new connection\n",client_sock->sensor_id);	//write log message tot FIFO
						client_sock->first_receive = TRUE;
						
						}
						if(sbuffer_insert(sbuffer, &data) != SBUFFER_SUCCESS) exit(EXIT_FAILURE);
						
      				}
					// if this sensor is closed manually we get the poll but no data, so we need to remove it from the sockets
					else
					{
						struct sock * temp_client = (struct sock *)dpl_get_element_at_index(sockets, i-1);
						ERR_HANDLER(temp_client == NULL, DPL_OP_FAIL);
						if(tcp_close(&(temp_client->socket))!= TCP_NO_ERROR) exit(EXIT_FAILURE);
						OBSERVE_PRINTF("The sensor node with %"PRIu16" has closed the connection\n",temp_client->sensor_id);
						WRITE_FIFO("The sensor node with %"PRIu16" has closed the connection\n",temp_client->sensor_id);
						dpl_remove_at_index(sockets, i-1, true);
						nfds--;
					}
			}
			}
			//here we remove the timed out connection
			if(flag_time_out_socket == TRUE)
			{
				int i = index_time_out_socket;
				struct sock * temp_client = (struct sock *)dpl_get_element_at_index(sockets,i);
				ERR_HANDLER(temp_client == NULL, DPL_OP_FAIL);
				//close this connection
				if(tcp_close(&(temp_client->socket))!= TCP_NO_ERROR) exit(EXIT_FAILURE);
				// remove with "true" flag so the strcut should be freed
				dpl_remove_at_index(sockets, i, true);
				// nfds-- because one connection is removed
				nfds--;
				OBSERVE_PRINTF("One connection is removed\n");
				OBSERVE_PRINTF("nfds= %d\n",nfds);
			}	
			//if no more socket is active, ready to shut down server
			if(dpl_size(sockets) == 0) 
			{
				nr_time_out++;
				OBSERVE_PRINTF("no more active clients\n");
				DEBUG_PRINTF("no more active clients\n");
				if(nr_time_out == 2) end_server = TRUE;
				//sleep(timeout-TIMEOUT) if the poll timeout and the socket timeout would be two different numbers 
			}
			} while(end_server == FALSE);
	//close server
	if (tcp_close( &server )!=TCP_NO_ERROR) exit(EXIT_FAILURE);
  	OBSERVE_PRINTF("Server is closed\n");
		DEBUG_PRINTF("Server is closed\n");
}

void connmgr_free()
{  
  	dpl_free(&sockets,true);
		OBSERVE_PRINTF("Connmgr shut down and freed\n");	
}


