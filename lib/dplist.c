#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"

/*
 * definition of error codes
 * */
#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
#define DPLIST_INVALID_ERROR 2 //error due to a list operation applied on a NULL list 

#ifdef DEBUG
	#define DEBUG_PRINTF(...) 									         \
		do {											         \
			fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	 \
			fprintf(stderr,__VA_ARGS__);								 \
			fflush(stderr);                                                                          \
                } while(0)
#else
	#define DEBUG_PRINTF(...) (void)0
#endif


#define DPLIST_ERR_HANDLER(condition,err_code)\
	do {						            \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");    \
            assert(!(condition));                                    \
        } while(0)

        
/*
 * The real definition of struct list / struct node
 */

struct dplist_node {
  dplist_node_t * prev, * next;
  void * element;
};

struct dplist {
  dplist_node_t * head;
  void * (*element_copy)(void * src_element);			  
  void (*element_free)(void ** element);
  int (*element_compare)(void * x, void * y);
};


dplist_t * dpl_create (// callback functions
			  void * (*element_copy)(void * src_element),
			  void (*element_free)(void ** element),
			  int (*element_compare)(void * x, void * y)
			  )
{
  dplist_t * list;
  list = malloc(sizeof(struct dplist));
  DPLIST_ERR_HANDLER(list==NULL,DPLIST_MEMORY_ERROR);
  list->head = NULL;  
  list->element_copy = element_copy;
  list->element_free = element_free;
  list->element_compare = element_compare; 
  return list;
}

void dpl_free(dplist_t ** list, bool free_element)
{
	int size = dpl_size(*list);
    DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
	DPLIST_ERR_HANDLER((*list==NULL),DPLIST_INVALID_ERROR);
	for(int i = 1; i <= size; i++)
    {
        dpl_remove_at_index(*list, 0,free_element); 
    }
    free(*list);
    *list = NULL;
}
dplist_t * dpl_insert_at_index(dplist_t * list, void * element, int index, bool insert_copy)
{
  dplist_node_t * ref_at_index, * list_node;
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
  list_node = malloc(sizeof(dplist_node_t));
  DPLIST_ERR_HANDLER((list_node==NULL),DPLIST_MEMORY_ERROR);
  assert(element != NULL);
  //element handling
  if(insert_copy == true)  list_node -> element = (*(list -> element_copy))(element);  //element_copy is pointing to a function address
  else if(insert_copy == false) list_node -> element = element;

  if (list->head == NULL)  //fist-time insert an element, initialize "head"
  {  
    list_node->prev = NULL;
    list_node->next = NULL;
    list->head = list_node;
  } else if (index <= 0)  // insert one node before head the new one is the new head now
  { 
    list_node->prev = NULL;
    list_node->next = list->head;
    list->head->prev = list_node;
    list->head = list_node;
  } else 
  {
    ref_at_index = dpl_get_reference_at_index(list, index);  
    assert( ref_at_index != NULL);
    if (index < dpl_size(list))
    { 
      list_node->prev = ref_at_index->prev;
      list_node->next = ref_at_index;
      ref_at_index->prev->next = list_node;
      ref_at_index->prev = list_node;
    } else
    { 
      assert(ref_at_index->next == NULL);
      list_node->next = NULL;
      list_node->prev = ref_at_index;
      ref_at_index->next = list_node;    
    }
  }
  return list;
}

dplist_t * dpl_remove_at_index( dplist_t * list, int index, bool free_element)
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
  dplist_node_t *ref_at_index, *dummy;
  if (list->head==NULL) return list;  // covers case 1
  ref_at_index = dpl_get_reference_at_index(list, index);
  assert(ref_at_index != NULL);
  if (ref_at_index == list->head)    //if it is the first element that is about to be removed
  { 
    dummy = list->head;
    list->head = list->head->next;
    if (list->head)  // there is at least another element
      list->head->prev = NULL;
  }
  else 
  { 
    assert( ref_at_index != NULL);
    dummy = ref_at_index;
    if (ref_at_index->next)
    {
	assert( ref_at_index != NULL);
      ref_at_index->next->prev = ref_at_index->prev;
      ref_at_index->prev->next = ref_at_index->next;
    } 
    else 
    {
      ref_at_index->prev->next = NULL; 
    }
  }
  if(free_element == true) (*(list->element_free))(&(dummy->element));   //free the element conditionally
  free(dummy);  
  return list; 
}

int dpl_size( dplist_t * list )
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
  int size = 0;
  dplist_node_t *dummy;
  dummy = list->head;
  while(dummy != NULL)
	{
		dummy = dummy->next;
		size+=1;
	}
  return size;
}

dplist_node_t * dpl_get_reference_at_index( dplist_t * list, int index )   
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
  int count;
  dplist_node_t * dummy;
  if (list->head == NULL ) return NULL;
  for ( dummy = list->head, count = 0; dummy->next != NULL ; dummy = dummy->next, count++) 
  { 
    if (count >= index) return dummy;
  }  
  return dummy;  
}

void * dpl_get_element_at_index( dplist_t * list, int index )
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
  dplist_node_t *dummy;
  if (list->head == NULL ) return (void *) 0;
  dummy = dpl_get_reference_at_index(list, index);
  return dummy -> element;
}

int dpl_get_index_of_element( dplist_t * list, void * element )
{
  DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
  int index;
  dplist_node_t * dummy;
  for ( dummy = list->head, index = 0; dummy != NULL; dummy = dummy->next, index++)
  {
	if((*(list->element_compare))(element,dummy->element) == 0) return index;    
  }
  return -1; // element not found
}

// HERE STARTS THE EXTRA SET OF OPERATORS //

// ---- list navigation operators ----//
  
dplist_node_t * dpl_get_first_reference( dplist_t * list )
{
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
	// If the list is empty, NULL is returned.
	if (list->head == NULL ) return NULL;
	// Returns a reference to the first list node of the list. 
	return list->head;
}

dplist_node_t * dpl_get_last_reference( dplist_t * list )
{
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
	if (list->head == NULL ) return NULL;
	return dpl_get_reference_at_index( list, dpl_size(list)-1 );
	
}

dplist_node_t * dpl_get_next_reference( dplist_t * list, dplist_node_t * reference )
{
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
	if(reference == NULL) return NULL;
	int index = dpl_get_index_of_reference( list, reference );
	if(index == -1) return NULL;
	dplist_node_t * dummy = dpl_get_reference_at_index( list, index );
	return dummy->next;
	
}

dplist_node_t * dpl_get_previous_reference( dplist_t * list, dplist_node_t * reference )
{
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
    dplist_node_t * dummy;
	if(reference == NULL) return dpl_get_last_reference( list );
	if (list->head == NULL ) return NULL;
	if(reference -> prev == NULL) return NULL;
	for(dummy = list->head; dummy != NULL; dummy = dummy->next)
	{
	  if(dummy == reference)
	  return reference->prev; 
	}
	return NULL; 
}

// ---- search & find operators ----//  
  
void * dpl_get_element_at_reference( dplist_t * list, dplist_node_t * reference )  
{
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
	dplist_node_t * dummy;
	if (list->head == NULL ) return NULL;
        if(reference == NULL) return dpl_get_last_reference(list) -> element;
	for(dummy = list->head; dummy != NULL; dummy = dummy->next)
	{
	  if(dummy == reference)
	  return reference -> element; 
	}
	return NULL; 
}

dplist_node_t * dpl_get_reference_of_element( dplist_t * list, void * element )  
{
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);	
	if (list->head == NULL ) return NULL;
	if(element == NULL) return NULL;
	int index = dpl_get_index_of_element( list, element );
	if(index != -1) return dpl_get_reference_at_index(list, index );
	return NULL; 
}

int dpl_get_index_of_reference( dplist_t * list, dplist_node_t * reference )  
{
    dplist_node_t * dummy;
	int count ;
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
	if (list->head == NULL ) return -1;
	if(reference == NULL) return dpl_size(list)-1;
	
	for(dummy = list->head,count = 0; dummy != NULL; dummy = dummy->next)
	{
	 
	  if((dummy->prev == reference->prev) && (dummy->next == reference->next) && (dummy->element == reference->element)) 
	  return count; 
	  count++;
	}
	return -1; 
}
  
// ---- extra insert & remove operators ----//

dplist_t * dpl_insert_at_reference( dplist_t * list, void * element, dplist_node_t * reference, bool insert_copy ) 
{	
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
	if(element == NULL) return list;
	if(reference == NULL) return dpl_insert_at_index(list, element, dpl_size(list), insert_copy);
    int index = dpl_get_index_of_reference(list, reference);
	if(index != -1) return dpl_insert_at_index(list, element, index, insert_copy);
	else return list;
}

dplist_t * dpl_insert_sorted( dplist_t * list, void * element, bool insert_copy ) 
{
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
	int count;
	dplist_node_t * dummy;
	if(list->head == NULL) dpl_insert_at_index(list, element, 0, insert_copy);
	for(dummy = list->head, count = 0; dummy != NULL; dummy = dummy->next, count++)
	{
		int compare = (*(list->element_compare))(element,dummy->element);
	   if( compare == 0   ) return list;
	  else if( compare == -1  ) return dpl_insert_at_index(list, element, count, insert_copy);
	   
	}
	return dpl_insert_at_index(list, element, dpl_size(list), insert_copy);

}

dplist_t * dpl_remove_at_reference( dplist_t * list, dplist_node_t * reference, bool free_element ) 
{
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
	if(list->head == NULL) return list;
	if(reference == NULL) return dpl_remove_at_index(list,  dpl_size(list)-1, free_element);
    	int index = dpl_get_index_of_reference(list, reference);
	if(index != -1) return dpl_remove_at_index(list, index, free_element);
	else 
	return list;
}

dplist_t * dpl_remove_element( dplist_t * list, void * element, bool free_element ) 
{
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);
	if(list->head == NULL) return list;
	int index = dpl_get_index_of_element(list,element);
    if(index!=-1)
    	dpl_remove_at_index( list, index, free_element);
    return list;
}

 
// ---- you can add your extra operators here ----//
void * dpl_look_for_element( dplist_t * list, void * element )
{
	dplist_node_t * dummy;
	DPLIST_ERR_HANDLER((list==NULL),DPLIST_INVALID_ERROR);

	if (list->head == NULL ) return NULL;
	for ( dummy = list->head; dummy->next != NULL ; dummy = dummy->next) 
	{ 
		if (!(list->element_compare(dummy->element, element))) return element;	//element compare returns 0 if equal
  	}
	if (!(list->element_compare(dummy->element, element))) return dummy->element;	//check last element in list
	return NULL;
}

