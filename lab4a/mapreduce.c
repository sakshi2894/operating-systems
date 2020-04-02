//
// Created by Sakshi Bansal on 02/04/20.
//


#include "mapreduce.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


// Method declarations and new data structures.
int get_thread_index(pthread_t *thread_pool, int size);

typedef struct ValueList {
  char* val;
  struct ValueList *next;
} ValueList;

typedef struct KeyList {
  char* key_name;
  struct KeyList *next;
  ValueList *valueList;
} KeyList;



// Global variables.
int mappers_num;
int reducers_num;
int file_num;
pthread_t *mapper_pool;
pthread_t *reducer_pool;
char** file_list;
int global_file_ptr = 0;
pthread_mutex_t file_mutex;
Mapper mapper;
Reducer reducer;
Partitioner partitioner;
Combiner combiner;
KeyList** combine_ds;
KeyList** reduce_ds;

// Method definitions.


KeyList* find_in_list(KeyList *head, char* data) {
  KeyList *temp = head;
  while (temp != NULL) {
    if (strcmp(temp -> key_name, data) == 0) {
      return temp;
    }
    temp = temp -> next;
  }
  return NULL;
}

char* copy_str(char *data) {
  char* copy = (char *) malloc((strlen(data) + 1) * sizeof(char)); //TODO: Free
  strcpy(copy, data);
  return copy;
}

KeyList* create_key_node(char* data) {
  KeyList *key = (KeyList *) malloc(sizeof(KeyList));
  key -> next = NULL;
  key -> key_name = copy_str(data);
  key->valueList  = NULL;
  return key;

}

ValueList* create_value_node(char* data) {
  ValueList *valueList = (ValueList *) malloc(sizeof(ValueList));
  valueList->next = NULL;
  valueList -> val = copy_str(data);
  return valueList;
}

void MR_EmitToCombiner(char *key, char *value) {
  //TODO: Explore pthread_getspecific()

  //printf("MR_EmitToCombiner\t key: %s\tvalue: %s\n", key, value);

  // push to combiner DS.
  int thread_idx = get_thread_index(mapper_pool, mappers_num);

  printf("Thread: %d\tMR_EmitToCombiner\t key: %s\tvalue: %s\n", thread_idx, key, value);
  KeyList* keys_head = combine_ds[thread_idx];
  ValueList* value_node = create_value_node(value);
  KeyList* key_node = find_in_list(keys_head, key);

  if (key_node == NULL) {
    //Create one.
    KeyList* new_node = create_key_node(key);
    new_node -> valueList = value_node;
    combine_ds[thread_idx] = new_node;
    new_node -> next = keys_head;
  } else {
    ValueList* value_head = key_node -> valueList;
    key_node -> valueList = value_node;
    value_node -> next = value_head;
  }
  //printf("Added new value for key: %s, value: %s\n", key, combine_ds[thread_idx]->valueList->val);

}

void MR_EmitToReducer(char *key, char *value) {
  //printf("MR_EmitToReducer\t key: %s\tvalue: %s\n", key, value);

  //int thread_idx = get_thread_index(reducer_pool, mappers_num);

  //TODO: Check conversion
  int partition_idx = partitioner(key, reducers_num);

  printf("Partition: %d\tMR_EmitToReducer\t key: %s\tvalue: %s\n", partition_idx, key, value);
  KeyList* keys_head = reduce_ds[partition_idx];
  ValueList* value_node = create_value_node(value);
  KeyList* key_node = find_in_list(keys_head, key);

  if (key_node == NULL) {
    //Create one.
    KeyList* new_node = create_key_node(key);
    new_node -> valueList = value_node;
    reduce_ds[partition_idx] = new_node;
    new_node -> next = keys_head;
  } else {
    ValueList* value_head = key_node -> valueList;
    key_node -> valueList = value_node;
    value_node -> next = value_head;
  }

}

unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
  unsigned long hash = 5381;
  int c;
  while ((c = *key++) != '\0')
    hash = hash * 33 + c;
  return hash % num_partitions;
}

int get_thread_index(pthread_t *thread_pool, int size) {
  int i;
  for (i = 0; i < size; i++) {
    if (pthread_equal(thread_pool[i], pthread_self())) {
      return i;
    }
  }
  printf("Thread not found\n");
  return -1;
}


char* CombineGetNext(char *key) {
  int thread_index = get_thread_index(mapper_pool, mappers_num);

  KeyList * keyList = combine_ds[thread_index];
  //printf("Combiner:\tValue in head key: %s\n", keyList->valueList->val);

  KeyList * keyNode = find_in_list(keyList, key);
  if (keyNode == NULL) {
    //printf ("Combiner:%d\tKey not present: %s\n", thread_index, key);
    return NULL;
  }
  //Start iterating over values.
  ValueList* valueNode = keyNode -> valueList;
  if (valueNode == NULL) {
    //printf ("Combiner:%d\tValue not present: %s\n", thread_index, key);
    return NULL;
  }

  char * retVal = copy_str(valueNode -> val);

  // Remove head of value list.
  keyNode -> valueList = valueNode -> next;
  free(valueNode->val);
  free(valueNode);

  //printf("Combiner:%d\tReturning value: %s\n", thread_index, retVal);
  return retVal;
}

char* ReduceGetNext(char *key, int partition_number) {

  KeyList * keyList = reduce_ds[partition_number];
  //printf("Combiner:\tValue in head key: %s\n", keyList->valueList->val);

  KeyList * keyNode = find_in_list(keyList, key);
  if (keyNode == NULL) {
    //printf ("Combiner:%d\tKey not present: %s\n", thread_index, key);
    return NULL;
  }
  //Start iterating over values.
  ValueList* valueNode = keyNode -> valueList;
  if (valueNode == NULL) {
    //printf ("Combiner:%d\tValue not present: %s\n", thread_index, key);
    return NULL;
  }

  char * retVal = copy_str(valueNode -> val);

  // Remove head of value list.
  keyNode -> valueList = valueNode -> next;
  free(valueNode->val);
  free(valueNode);

  //printf("Combiner:%d\tReturning value: %s\n", thread_index, retVal);
  return retVal;
}

void *combiner_wrapper() {
  int thread_index = get_thread_index(mapper_pool, mappers_num);
  KeyList * keyList = combine_ds[thread_index];
  while (keyList != NULL) {
    //printf("Combining key: %s\n", keyList -> key_name);
    combiner(keyList->key_name, CombineGetNext);
    // We are done with this key, moving onto the next.
    keyList = keyList -> next;
  }
  return NULL;
}

// Produce first list of key value pairs
void* map_wrapper() {
  while (1) {
    // Since curr_file_index will only increase.
    // No point in taking the lock.

    //TODO: Check this.
    if (global_file_ptr >= file_num) return NULL;
    int fi;

    pthread_mutex_lock(&file_mutex);
    fi = global_file_ptr;
    if (fi >= file_num) return NULL;
    global_file_ptr++;
    pthread_mutex_unlock(&file_mutex);

    // Run map function.
    mapper(file_list[fi]);

    // Figure out combiner here.
    if (combiner != NULL) {
      combiner_wrapper();
    }
   }
}

void* reduce_wrapper() {
  int thread_index = get_thread_index(reducer_pool, reducers_num);
  KeyList * keyList = reduce_ds[thread_index];
  while (keyList != NULL) {
    //printf("Combining key: %s\n", keyList -> key_name);
    reducer(keyList->key_name, NULL, ReduceGetNext, thread_index);
    // We are done with this key, moving onto the next.
    keyList = keyList -> next;
  }
  return NULL;
}

void init_map_threads() {
  mapper_pool = (pthread_t *) malloc(mappers_num * sizeof(pthread_t));    //TODO: Free
  int i = 0;
  for (i = 0; i < mappers_num; i++) {
    pthread_create(&mapper_pool[i], NULL, map_wrapper, NULL);
  }
  printf("Mapper pool created\n");
}

void wait_threads(pthread_t* thread_pool, int size) {
  int i;
  for (i = 0; i < size; i++) {
    pthread_join(thread_pool[i], NULL);
  }
  printf("Mappers are done.\n");
}

void init_reduce_threads() {
  reducer_pool = (pthread_t *) malloc(reducers_num * sizeof(pthread_t));  //TODO: Free
  int i = 0;
  for (i = 0; i < reducers_num; i++) {
    pthread_create(&reducer_pool[i], NULL, reduce_wrapper, NULL);
  }
  printf("Reducer pool created\n");
}

void init_combine_ds() {
    combine_ds = (KeyList **) malloc(mappers_num * sizeof(KeyList *));
}

void init_reduce_ds() {
    reduce_ds = (KeyList **) malloc(reducers_num * sizeof(KeyList *));
}

void MR_Run(int argc, char *argv[],
            Mapper map, int num_mappers,
            Reducer reduce, int num_reducers,
            Combiner combine,
            Partitioner partition) {

  // Initialise num_mappers, num_reducers, mapper, reducer, combine and partitioner globally.
  mappers_num = num_mappers;
  reducers_num = num_reducers;
  mapper = map;
  reducer = reduce;
  combiner = combine;
  partitioner = partition;
  file_num = argc - 1;
  reducer = reduce;
  partitioner = partition;


  // Init file list.
  file_list = (char**) malloc(file_num * sizeof(char *));    //TODO: Free
  int i;
  for (i = 1; i < argc; i++) {
    file_list[i - 1] = copy_str(argv[i]);
    printf("filename: %s\n", file_list[i - 1]);
  }

  // Init data structures for combine phase.
  init_combine_ds();

  // Init data structures for reduce phase.
  init_reduce_ds();

  // Init lock before map threads.
  pthread_mutex_init(&file_mutex, NULL);

  // Init mapper threads.
  init_map_threads();

  // Wait for mapper threads to finish.
  wait_threads(mapper_pool, mappers_num);

  // Init reducer threads.
  init_reduce_threads();

  // Wait for reducer threads to finish.
  wait_threads(reducer_pool, reducers_num);
}
