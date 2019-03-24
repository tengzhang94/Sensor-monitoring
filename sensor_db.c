#include "config.h" 
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <unistd.h>
#include "sensor_db.h"


/* when DB connection fails we wait a bit
 * then reconnect, after 3 attempts we close
*/
#define conn_attempt_wait 0.3

/*If the table existed, clear up the existing data if clear_up_flag is set to 1 */
DBCONN * init_connection(char clear_up_flag)
{
 	DBCONN *conn;
	char *err_msg = 0;
	char *sql;
  	int rc = sqlite3_open(TO_STRING(DB_NAME), &conn);
  	if (rc != SQLITE_OK) 
	{
    	DEBUG_PRINTF("Cannot open database: %s\n", sqlite3_errmsg(conn));
		WRITE_FIFO("Cannot open database: %s\n", sqlite3_errmsg(conn));  
		sqlite3_close(conn);

    free(err_msg);
    return NULL;
    }
	DEBUG_PRINTF("Connection to SQL server established\n");
	WRITE_FIFO("Connection to SQL server established\n");
	/* when clear_up is needed */
  	if(clear_up_flag == 1)  
    {  
  	asprintf(&sql,"DROP TABLE IF EXISTS %s;", TO_STRING(TABLE_NAME));
  	rc = sqlite3_exec(conn, sql, 0, 0, &err_msg);	
	if(rc!= SQLITE_OK)
	{
		DEBUG_PRINTF("Cannot drop the table: %s\n",sqlite3_errmsg(conn));
		sqlite3_close(conn);
		free(sql);
		sqlite3_free(err_msg);
		return NULL;
	}
	OBSERVE_PRINTF("Table dropped\n");
	DEBUG_PRINTF("Table dropped\n");
  	free(sql);
    }
	asprintf(&sql, "CREATE TABLE %s(Id INTEGER PRIMARY KEY, sensor_id INT, sensor_value DECIMAL(4,2), timestamp TIMESTAMP);", TO_STRING(TABLE_NAME));
    rc = sqlite3_exec(conn, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK ) 
	{
    DEBUG_PRINTF("Initialization: %s\n", sqlite3_errmsg(conn));
    sqlite3_free(err_msg);
	free(sql);
	return conn;
	} 
	else 
	{
		WRITE_FIFO("New table %s created\n", TO_STRING(TABLE_NAME));
		OBSERVE_PRINTF("New table %s created\n", TO_STRING(TABLE_NAME));
		DEBUG_PRINTF("New table %s created\n", TO_STRING(TABLE_NAME));
	}		    
	free(sql);
	free(err_msg);
   	return conn;
}

void storagemgr_parse_sensor_data(DBCONN * conn, sbuffer_t ** buffer)
{
	printf("HERE WE CAN SEE IF THE VARIABLE FORTEST IS ACCESSIBLE IN STORAGEMGR: %d\n", forTest);
	int nr_conn_fail = 0;
	sbuffer_t * buffer_temp = (sbuffer_t *)*buffer;
	sensor_data_t * data = malloc(sizeof(sensor_data_t));
	ERR_HANDLER(data == NULL, ERROR_MEMORY);
	while(sbuffer_remove(buffer_temp, data) == SBUFFER_SUCCESS)
	{
	OBSERVE_PRINTF("The above sensor data read by storagemgr\n");
	sensor_id_t id = data->id;
	sensor_value_t value = data->value;
	sensor_ts_t ts = data->ts;
	/*check if DB connection fails*/
		while (insert_sensor(conn,id, value, ts) == 1)
		{
			if(nr_conn_fail == 3)
			{
			/* if 3 failed attempts, no more wait 
			and close this whole manager*/
			DEBUG_PRINTF("3 attempts failed = CONNECTION LOST");
			WRITE_FIFO("Connection to SQL server lost\n");
			free(data);
			return;
		}
		sleep(conn_attempt_wait);
		nr_conn_fail++;
		}
  	}
	OBSERVE_PRINTF("Storagemgr: NO DATA IN THE BUFFER or BUFFER IS NULL\n");
	free(data);
}

void disconnect(DBCONN *conn)
{ 	
	OBSERVE_PRINTF("Connection to SQL server lost\n");
	DEBUG_PRINTF("Connection to SQL server lost\n");
	WRITE_FIFO("Connection to SQL server lost\n");
	sqlite3_close(conn);
}

int insert_sensor(DBCONN * conn, sensor_id_t id, sensor_value_t value, sensor_ts_t ts)
{
  	char *err_msg = 0;
	char* sql;
  	asprintf(&sql,"INSERT INTO %s VALUES(NULL,%f,%f,%f)", TO_STRING(TABLE_NAME),(double)id,(double)value, (double)ts); 

  	int rc = sqlite3_exec(conn, sql, 0, 0, &err_msg);
  	free(sql); 
	//sqlite3_free(err_msg);
  	if (rc == SQLITE_OK )  
  	{
	OBSERVE_PRINTF("Sensor data inserted\n");
  	return 0; //successful,return 0
  	}
  	DEBUG_PRINTF("Error: %s\n",sqlite3_errmsg(conn));
	sqlite3_free(err_msg);
  	return 1; //fail, return 1
}

/*
int insert_sensor_from_file(DBCONN * conn, FILE * sensor_data)
{
  	sensor_value_t value=0.0; 
	sensor_id_t id; 
	sensor_ts_t ts;
  	while(!feof(sensor_data))
  	{
   	//fread(&id,sizeof(sensor_id_t),1,sensor_data);
   	//fread(&value,sizeof(sensor_value_t),1,sensor_data);
   	//fread(&ts,sizeof(sensor_ts_t),1,sensor_data);
   	if((fread(&id,sizeof(sensor_id_t),1,sensor_data)==0) || (fread(&value,sizeof(sensor_value_t),1,sensor_data)==0) || (fread(&ts,sizeof(sensor_ts_t),1,sensor_data)==0))
  	{
	fprintf(stdout,"Cannot read data from the FILE\n");
  	return 1;
  	}
  	else
	{
   	fread(&id,sizeof(sensor_id_t),1,sensor_data);
   	fread(&value,sizeof(sensor_value_t),1,sensor_data);
   	fread(&ts,sizeof(sensor_ts_t),1,sensor_data);
   	//insert_sensor(conn, id,  value, ts);
	} //data read successfully
	
  	if(insert_sensor(conn, id,  value, ts) == 0)
	{
	fprintf(stdout,"Data from the FILE inserted\n");
  	
	}
	else
	{
	fprintf(stderr,"Cannot insert data\n");
	return 1;
	}
  	}
 	return 0;
}
*/
int find_sensor_all(DBCONN * conn, callback_t f) //return all sensor measurements 
{
  	char *err_msg = 0;
	char* sql;
  	asprintf(&sql,"SELECT * FROM %s;", TO_STRING(TABLE_NAME));

  	int rc = sqlite3_exec(conn, sql, f, 0, &err_msg); // third parameter is for call back function
  	free(sql); 
  	if (rc != SQLITE_OK )
  	{
	DEBUG_PRINTF("Error: %s\n",sqlite3_errmsg(conn));
  	sqlite3_free(err_msg);
  	return 1;
  	}
	
	sqlite3_free(err_msg);
  	return 0;
}

int find_sensor_by_value(DBCONN * conn, sensor_value_t value, callback_t f)
{
  	char *err_msg = 0;
	char* sql;
  	asprintf(&sql,"SELECT * FROM %s WHERE sensor_value=%g", TO_STRING(TABLE_NAME), value);

  	int rc = sqlite3_exec(conn, sql, f, 0, &err_msg);
  	free(sql); 
  	if (rc != SQLITE_OK )
  	{
	DEBUG_PRINTF("Error: %s\n",err_msg);
  	sqlite3_free(err_msg);
  	return 1;
  	}
	sqlite3_free(err_msg);
  	return 0;
}

int find_sensor_exceed_value(DBCONN * conn, sensor_value_t value, callback_t f)
{
  	char *err_msg = 0;
	char* sql;
  	asprintf(&sql, "SELECT * FROM %s WHERE sensor_value>%g", TO_STRING(TABLE_NAME), value);

  	int rc = sqlite3_exec(conn, sql, f, 0, &err_msg);
  	free(sql); 
  	if (rc != SQLITE_OK )
  	{
	DEBUG_PRINTF("Error: %s",err_msg);
  	sqlite3_free(err_msg);
  	return 1;
  	}
	sqlite3_free(err_msg);
  	return 0;
}

int find_sensor_by_timestamp(DBCONN * conn, sensor_ts_t ts, callback_t f)
{
  	char *err_msg = 0;
	char *sql;
  	asprintf(&sql,"SELECT * FROM %s WHERE timestamp=%ld;", TO_STRING(TABLE_NAME), ts);

  	int rc = sqlite3_exec(conn, sql, f, 0, &err_msg);
  	free(sql); 
  	if (rc != SQLITE_OK )
  	{
	DEBUG_PRINTF("Error: %s",err_msg);
  	sqlite3_free(err_msg);
  	return 1;
  	}
	sqlite3_free(err_msg);
  	return 0;
}

int find_sensor_after_timestamp(DBCONN * conn, sensor_ts_t ts, callback_t f)
{
  	char *err_msg = 0;
	char* sql;
  	asprintf(&sql,"SELECT * FROM %s WHERE timestamp>%ld;", TO_STRING(TABLE_NAME), ts);

  	int rc = sqlite3_exec(conn, sql, f, 0, &err_msg);
  	free(sql); 
  	if (rc != SQLITE_OK )
  	{
	DEBUG_PRINTF("Error: %s",err_msg);
  	sqlite3_free(err_msg);
  	return 1;
  	}
	sqlite3_free(err_msg);
  	return 0;
}
