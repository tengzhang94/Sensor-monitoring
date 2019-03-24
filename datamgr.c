#include "config.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include "datamgr.h"

dplist_t * sensor_node_list = NULL;
dplist_node_t * get_reference_by_id(sensor_id_t sensor_id);
sensor_element_t * get_element_by_id(sensor_id_t sensor_id);
void  dpl_print(dplist_t * list );
void * sensor_element_copy(void * element);
void sensor_element_free(void ** element);
int sensor_element_compare(void * x, void * y);

struct dplist_node
{
    dplist_node_t *prev, *next;
    void * element;
};

void datamgr_parse_sensor_data(FILE * fp_sensor_map, sbuffer_t ** buffer)
{
	printf("HERE WE CAN SEE IF THE VARIABLE FORTEST IS ACCESSIBLE IN DATAMGR: %d\n",forTest);
    sbuffer_t * buffer_temp = (sbuffer_t *) *buffer;
    sensor_element_t sensor_element;
    sensor_element_t * element_dummy = NULL;
    sensor_data_t sensor_data;
    int k;
    //create a list to put sensor_element
    sensor_node_list = dpl_create(sensor_element_copy, sensor_element_free, sensor_element_compare);
	ERR_HANDLER(sensor_node_list == NULL, DPL_OP_FAIL);
    sensor_element.sensor_id = 0;
    sensor_element.room_id = 0;
    sensor_element.count = 0;    
    sensor_element.avg = 0;
    sensor_element.timestamp = 0;
    
    for(k = 0; k < RUN_AVG_LENGTH; k++){
    sensor_element.last_values[k] = 0;
    } 
	// read fp_sensor_map to put all sensor nodes in the list
    while (fscanf(fp_sensor_map,"%" SCNu16 " %" SCNu16 "\n", &sensor_element.room_id, &sensor_element.sensor_id) != EOF) 
	{
        //dpl_insert_sorted(sensor_node_list, (void*)&sensor_element, true);
		ERR_HANDLER(dpl_insert_sorted(sensor_node_list, (void*)&sensor_element, true) == NULL, DPL_OP_FAIL);
    }
	while(sbuffer_remove(buffer_temp, &sensor_data) == SBUFFER_SUCCESS)		
	{
        OBSERVE_PRINTF("The above sensor data read by datamgr\n");
		element_dummy = get_element_by_id(sensor_data.id);
        if (element_dummy != NULL) 
		{
            element_dummy->last_values[element_dummy->count % RUN_AVG_LENGTH] = sensor_data.value;
            element_dummy->count++;
            element_dummy->timestamp = sensor_data.ts;
			if (element_dummy->count >= RUN_AVG_LENGTH) 
			{
            element_dummy->avg = 0;
            for ( k = 0; k < RUN_AVG_LENGTH; k++) 
			{
                element_dummy->avg += element_dummy->last_values[k];
            }
            element_dummy->avg = element_dummy->avg/RUN_AVG_LENGTH;
            if (element_dummy->avg > SET_MAX_TEMP) 
			{
				WRITE_FIFO("The senor node with %" PRIu16 " reports it's too hot(running avg temperature = %lf)\n", element_dummy->sensor_id, element_dummy->avg);
				OBSERVE_PRINTF("The senor node with %" PRIu16 " reports it's too hot(running avg temperature = %lf)\n", element_dummy->sensor_id, element_dummy->avg);			
         	}
            if (element_dummy->avg < SET_MIN_TEMP) 
			{
				WRITE_FIFO("The senor node with %" PRIu16 " reports it's too cold(running avg temperature = %lf)\n", element_dummy->sensor_id, element_dummy->avg);
				OBSERVE_PRINTF("The senor node with %" PRIu16 " reports it's too cold(running avg temperature = %lf)\n", element_dummy->sensor_id, element_dummy->avg);
			}
        	}
        }
		else // invalid sensor node
		{	
            WRITE_FIFO("Received sensor data with invalid sensor node ID %" PRIu16 "\n", sensor_data.id);
			OBSERVE_PRINTF("Received sensor data with invalid sensor node ID %" PRIu16 "\n", sensor_data.id);
		}
	}
	OBSERVE_PRINTF("Datamgr: NO DATA IN THE BUFFER or BUFFER IS NULL\n");
	fclose(stderr);    
}

void datamgr_free()
{
    dpl_free(&sensor_node_list,true);
	sensor_node_list = NULL;
	OBSERVE_PRINTF("Datamgr list free'd\n");
	DEBUG_PRINTF("Datamgr list free'd\n");
}

uint16_t datamgr_get_room_id(sensor_id_t sensor_id)
{
    sensor_element_t * element_dummy = get_reference_by_id(sensor_id)->element;
    if(element_dummy != NULL) return element_dummy->room_id;
    else return -1;
}

sensor_value_t datamgr_get_avg(sensor_id_t sensor_id)
{
    sensor_element_t * element_dummy = get_reference_by_id(sensor_id)->element;
    if(element_dummy != NULL) return element_dummy->avg;
    return 0;
}

time_t datamgr_get_last_modified(sensor_id_t sensor_id)
{
    sensor_element_t * element_dummy = get_reference_by_id(sensor_id)->element;
    if(element_dummy != NULL) return element_dummy->timestamp;
    else return 0;
}

int datamgr_get_total_sensors()
{
    return dpl_size(sensor_node_list);
}

sensor_element_t * get_element_by_id(sensor_id_t sensor_id)
{
	sensor_id_t id = sensor_id;
	dplist_node_t * dummy;
	dummy = get_reference_by_id(id);
	if(dummy == NULL) return NULL;
	else return dummy->element; 
}

dplist_node_t * get_reference_by_id(sensor_id_t sensor_id)
{
	dplist_node_t * dummy;
	dummy = dpl_get_first_reference(sensor_node_list);
	if (dummy == NULL) return NULL;
	while(dummy != NULL)
	{
		if(((sensor_element_t *)(dpl_get_element_at_reference(sensor_node_list, dummy)))->sensor_id == sensor_id)
		return dummy ;
		dummy = dummy ->next;
	}
	return NULL;	
}

void  dpl_print(dplist_t * list )
{
	int i, length;
    length = dpl_size(list);
    for (i = 0; i < length; i++) 
	{
 	sensor_element_t *element = dpl_get_element_at_index(list, i);
    printf(" Room_id %" PRIu16 " with Sensor id %" PRIu16" value %f ,last ts %ld\n",	element->room_id, element->sensor_id,element->avg, element->timestamp);
    }
}
/** call back functions **/
void * sensor_element_copy(void * element) {
    sensor_element_t* copy = malloc(sizeof (sensor_element_t));
	ERR_HANDLER(copy == NULL, ERROR_MEMORY);
    copy->sensor_id = ((sensor_element_t*)element)->sensor_id;
    copy->room_id = ((sensor_element_t*)element)->room_id;
	copy->count = 0;    
    copy->avg = 0;
    copy->timestamp = 0;
    
    for(int k = 0; k < RUN_AVG_LENGTH; k++){
    copy->last_values[k] = 0;
    } 
    return (void *) copy;
}

void sensor_element_free(void ** element) {
    free(*element);
    *element = NULL;
}

int sensor_element_compare(void * x, void * y) {
    return ((((sensor_element_t*)x)->sensor_id < ((sensor_element_t*)y)->sensor_id) ? -1 : (((sensor_element_t*)x)->sensor_id == ((sensor_element_t*)y)->sensor_id) ? 0 : 1);
}

