#include "mapreduce.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NUM_KEY_LISTS 997

// Structure representing a node in Value list
typedef struct ValueNode {
  char *value;
  struct ValueNode *next_value;
} ValueNode;

// Structure representing a node in Key list
typedef struct KeyNode {
  char *key;
  ValueNode *value_list_head;
  struct KeyNode *next_key;
} KeyNode;

typedef struct KeyValueNode {
  char *key;
  char *value;
  struct KeyValueNode *next;
} KeyValueNode;

//Map state variables
int num_files;
char **filenames;
int f_index;
pthread_mutex_t f_index_lock;
int map_workers;
pthread_t *map_threads;
KeyNode ***key_lists;
int mappers_done;
Mapper map_fun;
Combiner combine_fun;
ValueNode* valueNodeToFree;
KeyValueNode* keyValueNodeToFree;

//Reduce state variables
pthread_mutex_t *buffer_locks;
sem_t *sem_filled_locks;
Partitioner partition_fun;
int reduce_workers;
pthread_t *reduce_threads;

KeyValueNode **buffer;
KeyValueNode **partial_lists;
Reducer reduce_fun;

// The Default Hash function
unsigned long MR_DefaultHashPartition(char *key, int num_partitions) {
  unsigned long hash = 5381;
  int c;
  while ((c = *key++) != '\0') hash = hash * 33 + c;
  return hash % num_partitions;
}

// Utility function to find the index of this map thread in pool
int FindMapperThreadIndex() {
  int i;
  for (i = 0; i < map_workers; i++) {
    if (pthread_equal(map_threads[i], pthread_self()))
      break;
  }
  return i;
}

// Utility function to find the index of this reduce thread in pool
int FindReducerThreadIndex() {
  int i;
  for (i = 0; i < reduce_workers; i++) {
    if (pthread_equal(reduce_threads[i], pthread_self()))
      break;
  }
  return i;
}

// Allocates a new Key Node with given key
KeyNode *AllocateKeyNode(char *key) {
  KeyNode *key_node = (KeyNode *)malloc(sizeof(KeyNode));
  key_node->key = (char *)malloc((strlen(key) + 1) * sizeof(char));
  strcpy(key_node->key, key);
  key_node->value_list_head = NULL;
  key_node->next_key = NULL;
  return key_node;
}

// Allocates a new Value Node with given value
ValueNode *AllocateValueNode(char *value) {
  ValueNode *value_node = (ValueNode *)malloc(sizeof(ValueNode));
  value_node->value = (char *)malloc((strlen(value) + 1) * sizeof(char));
  strcpy(value_node->value, value);
  value_node->next_value = NULL;
  return value_node;
}

KeyValueNode *AllocateKeyValueNode(char *key, char *value) {
  KeyValueNode *key_value_node = (KeyValueNode *)malloc(sizeof(KeyValueNode));
  key_value_node->key = (char *)malloc((strlen(key) + 1) * sizeof(char));
  strcpy(key_value_node->key, key);

  key_value_node->value = (char *)malloc((strlen(value) + 1) * sizeof(char));
  strcpy(key_value_node->value, value);

  key_value_node->next = NULL;
  return key_value_node;
}

// Returns the node with given key if exists
KeyNode *FindKeyNode(KeyNode *key_head, char *key) {
  while (key_head != NULL) {
    if (strcmp(key, key_head->key) == 0)
      break;
    key_head = key_head->next_key;
  }
  return key_head;
}

// Returns the node with given key if exists
KeyValueNode *FindKeyValueNode(KeyValueNode *key_value_head, char *key) {
  while (key_value_head != NULL) {
    if (strcmp(key, key_value_head->key) == 0)
      break;
    key_value_head = key_value_head->next;
  }
  return key_value_head;
}

void FreeStringEnd(char* data) {
  ValueNode* node = (ValueNode *) malloc(sizeof(ValueNode));
  node -> value = data;
  node -> next_value = NULL;

  if (valueNodeToFree == NULL) {
    valueNodeToFree = node;
  } else {
    node->next_value = valueNodeToFree;
    valueNodeToFree = node;
  }

}

// Initializing the Hash Table for each Map thread
void AllocateMapHashTable() {
  key_lists = (KeyNode ***)malloc(map_workers * sizeof(KeyNode **));  //Freed
  int i, j;
  for (i = 0; i < map_workers; i++) {
    key_lists[i] = (KeyNode **)malloc(NUM_KEY_LISTS * sizeof(KeyNode *));  //Freed
    for (j = 0; j < NUM_KEY_LISTS; j++) {
      key_lists[i][j] = NULL;
    }
  }
  //printf("Initialized Mapper Hash Table...\n");
}

// Freeing up memory used by Hash Table of each Map thread
void FreeMapHashTable() {
  int i;
  for (i = 0; i < map_workers; i++) {
    free(key_lists[i]);
  }
  free(key_lists);
}

void FreeReduceHashTable() {
  int i;
  for (i = 0; i < reduce_workers; i++) {
    free(partial_lists[i]);
    free(buffer[i]);
  }
  free(partial_lists);
  free(buffer);
}

// Adding key and value to the hash table
void MR_EmitToCombiner(char *key, char *value) {
  //printf("Emitted to Combiner, K: %s, V: %s\n", key, value);
  int worker_idx = FindMapperThreadIndex();
  int hash_idx = MR_DefaultHashPartition(key, NUM_KEY_LISTS);
  //printf("worker: %d, hash: %d\n", worker_idx, hash_idx);
  KeyNode *key_node = FindKeyNode(key_lists[worker_idx][hash_idx], key);
  if (key_node == NULL) {
    // Inserting new key at front
    key_node = AllocateKeyNode(key);  //Freed
    key_node->next_key = key_lists[worker_idx][hash_idx];
    key_lists[worker_idx][hash_idx] = key_node;
  }
  // Inserting value count at front
  ValueNode *value_node = AllocateValueNode(value);  //Freed
  value_node->next_value = key_node->value_list_head;
  key_node->value_list_head = value_node;
}

// getnext function for Combine
char *CombineIterator(char *key) {
  //printf("combineIterator for %s\n", key);
  int worker_idx = FindMapperThreadIndex();
  int hash_idx = MR_DefaultHashPartition(key, NUM_KEY_LISTS);
  //printf("# %d %d\n", worker_idx, hash_idx);
  KeyNode *key_node = FindKeyNode(key_lists[worker_idx][hash_idx], key);
  if (key_node == NULL)
    return NULL;
  ValueNode *value_list_head = key_node->value_list_head;
  //printf("@ %s\n", key_node->key);
  if (value_list_head == NULL)
    return NULL;
  ValueNode *next_value = value_list_head->next_value;
  char *value = value_list_head->value;
  //printf("! %s\n", value);
  //free(iterator->value);  //TODO Modify free here. should be done here
  FreeStringEnd(value);
  free(value_list_head);
  key_node->value_list_head = next_value;
  return value;
}

// For each unique key, call combine
void IterateKeysToCombine() {
  int worker_idx = FindMapperThreadIndex();
  KeyNode *key_head;
  int i;
  for (i = 0; i < NUM_KEY_LISTS; i++) {
    while ((key_head = key_lists[worker_idx][i]) != NULL) {
      KeyNode *next_key = key_head->next_key;
      //printf("Combining key: %s from worker: %d\n", key_head->key, FindMapperThreadIndex());
      combine_fun(key_head->key, &CombineIterator);
      free(key_head->key);
      free(key_head);
      key_lists[worker_idx][i] = next_key;
    }
  }
}

// Wrapper function for the Mapper threads
void *MapWrapper(void *args) {
  while (1) {
    int idx;
    pthread_mutex_lock(&f_index_lock);
    idx = f_index;
    if (f_index < num_files)
      f_index++;
    pthread_mutex_unlock(&f_index_lock);
    if (idx == num_files) {
      // All files have been processed
      //printf("Mapper/Combiner %d exiting\n", FindMapperThreadIndex());
      break;
    }
    map_fun(filenames[idx]);
    //printf("Mapper for file no: %d completed\n", idx);
    if (combine_fun != NULL) {
      IterateKeysToCombine();
    }
    //printf("Combiner for file no: %d completed\n", idx);
  }
  return NULL;
}

// Adds to buffer DS
void MR_EmitToReducer(char *key, char *value) {
  unsigned long reduce_hash_idx = partition_fun(key, reduce_workers);
  //printf("Emitted to Reducer, K: %s, V: %s, W: %ld\n", key, value, reduce_hash_idx);

  KeyValueNode *key_value_node = AllocateKeyValueNode(key, value);  // Freed
  pthread_mutex_lock(&buffer_locks[reduce_hash_idx]);

  //printf("uploaded %s and %s\n", key, value);

  // Produce
  key_value_node->next = buffer[reduce_hash_idx];
  buffer[reduce_hash_idx] = key_value_node;

  sem_post(&sem_filled_locks[reduce_hash_idx]);
  pthread_mutex_unlock(&buffer_locks[reduce_hash_idx]);
}

// Adds to partial_lists DS
void MR_EmitReducerState(char *key, char *state, int partition_number) {
  KeyValueNode *keyValueNode = FindKeyValueNode(partial_lists[partition_number], key);
  if (keyValueNode != NULL) {
    //Update it with new value.
    free(keyValueNode->value);
    keyValueNode->value = (char *)malloc((strlen(state) + 1) * sizeof(char));
    strcpy(keyValueNode->value, state);
  } else {
    // Else add a new node with given value.;
    //printf("Creating new node  for key: %s, pnum: %d\n", key, partition_number);
    KeyValueNode *newNode = AllocateKeyValueNode(key, state);  // Freed
    newNode->next = partial_lists[partition_number];
    partial_lists[partition_number] = newNode;
  }
}

// getstate function for Reducer
char *ReduceStateIterator(char *key, int partition_number) {
  // Find the key in the data structure.
  //printf("RSI, key: %s, partition_number: %d\n", key, partition_number);
  KeyValueNode *keyValueNode = FindKeyValueNode(partial_lists[partition_number], key);
  if (keyValueNode == NULL)  //TODO check if this cond can be removed
    return NULL;
  //printf("RSI value: %s, partition_number: %d\n", keyValueNode->value, partition_number);
  //FreeStringEnd(keyValueNode->value);
  return keyValueNode->value;
}

// getnext() function for Reducer
char *ReduceIterator(char *key, int partition_number) {
  char *value = NULL;
  pthread_mutex_lock(&buffer_locks[partition_number]);
  //printf("Got lock in iterator\n");
  if (mappers_done != 1 || buffer[partition_number] != NULL) {
    KeyValueNode *keyValueNode = buffer[partition_number];
    KeyValueNode *prev = NULL;

    while (keyValueNode != NULL) {
      if (strcmp(keyValueNode->key, key) == 0) {
        if (prev == NULL) {  //This is the first node.
          buffer[partition_number] = keyValueNode->next;
        } else {
          prev->next = keyValueNode->next;
        }
        value = keyValueNode->value;
        //free(keyValueNode->key);
        FreeStringEnd(value);
        free(keyValueNode);
        break;
      } else {
        prev = keyValueNode;
        keyValueNode = keyValueNode->next;
      }
    }
  }
  pthread_mutex_unlock(&buffer_locks[partition_number]);

  return value;
}

// For each unique key, call reduce
void *ReduceWrapper(void *args) {
  int idx = FindReducerThreadIndex();
  //printf("# %d\n", idx);
  while (1) {
    //printf("Reducer Number %d waiting for data\n", idx);
    sem_wait(&sem_filled_locks[idx]);
    if (mappers_done == 1 && buffer[idx] == NULL) {
      //Run reduce for all the keys of this partition
      //printf("All mappers are done, exiting now by: %d\n", idx);
      KeyValueNode *key_value_head;
      while ((key_value_head = partial_lists[idx]) != NULL) {
        KeyValueNode *next_key_value = key_value_head->next;
        reduce_fun(key_value_head->key, &ReduceStateIterator, &ReduceIterator, idx);
        free(key_value_head->key);
        FreeStringEnd(key_value_head->value);
        free(key_value_head);
        partial_lists[idx] = next_key_value;
      }
      //printf("Reducer %d exiting\n", idx);
      break;
    }
    //printf("Reducer Number %d has data\n", idx);
    //printf("downloaded %s and %s\n", buffer[idx]->key, buffer[idx]->value);
    //reduce_fun(buffer[idx]->key, &ReduceStateIterator, &ReduceIterator, idx);
    //free(buffer[idx]);
    //buffer[idx] = NULL;
    //printf("Reducer Number %d has finished processing its data\n", idx);

    //printf("Waiting for mutex\n");
    pthread_mutex_lock(&buffer_locks[idx]);
    //printf("Got lock\n");
    char *key = buffer[idx]->key;
    pthread_mutex_unlock(&buffer_locks[idx]);

    reduce_fun(key, &ReduceStateIterator, &ReduceIterator, idx);
    free(key);
  }

  //free(buffer[idx]);
  //buffer[idx] = NULL;
  return NULL;
}

// Freeing resources used my Mappers
void FreeMapResources() {
  free(map_threads);
  FreeMapHashTable();

  free(filenames);
  while (valueNodeToFree != NULL) {
    ValueNode* next = valueNodeToFree->next_value;
    free(valueNodeToFree->value);
    free(valueNodeToFree);
    valueNodeToFree = next;
  }
}

void FreeReduceResources() {
  free(reduce_threads);

  free(sem_filled_locks);
  free(buffer_locks);
  FreeReduceHashTable();
}

// Creating workers to run Reducers
void RunReducerThreadPool() {
  reduce_threads = (pthread_t *)malloc(reduce_workers * sizeof(pthread_t));  //Freed
  int i;
  for (i = 0; i < reduce_workers; i++) {
    pthread_create(&reduce_threads[i], NULL, ReduceWrapper, NULL);
  }
  //printf("Launched all reducer threads...\n");
}

// Creating workers to run Mappers
void RunMapperThreadPool() {
  pthread_mutex_init(&f_index_lock, NULL);
  map_threads = (pthread_t *)malloc(map_workers * sizeof(pthread_t));  //Freed
  int i;
  for (i = 0; i < map_workers; i++) {
    pthread_create(&map_threads[i], NULL, MapWrapper, NULL);
  }
  //printf("Launched all mapper threads...\n");
}

//Waits for Mappers to finish executing
void WaitForMappersToComplete() {
  int i;
  for (i = 0; i < map_workers; i++) {
    pthread_join(map_threads[i], NULL);
  }
  //printf("Mappers are done\n");
  mappers_done = 1;
}

// Wakes up reducers to terminate
void WaitForReducersToComplete() {
  int i;
  for (i = 0; i < reduce_workers; i++) {
    //printf("Waiting for reducer %d\n", i);
    sem_post(&sem_filled_locks[i]);
    pthread_join(reduce_threads[i], NULL);
  }
  //printf("Reducers are done\n");
}

// Assigning values to the global variables
void InitializeGlobalVariables(int argc, char *argv[], int num_mappers, int num_reducers,
                               Partitioner partition, Mapper map, Combiner combine,
                               Reducer reduce) {
  map_workers = num_mappers;
  reduce_workers = num_reducers;
  partition_fun = partition;
  map_fun = map;
  combine_fun = combine;
  reduce_fun = reduce;
  f_index = 0;
  mappers_done = 0;

  num_files = argc - 1;
  filenames = (char **)malloc(num_files * sizeof(char *));  //Freed
  int i;
  for (i = 0; i < num_files; i++) {
    filenames[i] = argv[i + 1];
  }

  buffer_locks = (pthread_mutex_t *)malloc(reduce_workers * sizeof(pthread_mutex_t));  // freed
  sem_filled_locks = (sem_t *)malloc(reduce_workers * sizeof(sem_t));                  // freed
  for (i = 0; i < reduce_workers; i++) {
    pthread_mutex_init(&buffer_locks[i], NULL);
    sem_init(&sem_filled_locks[i], 0, 0);
  }
  buffer = (KeyValueNode **)malloc(reduce_workers * sizeof(KeyValueNode *));
  for (i = 0; i < reduce_workers; i++) {
    buffer[i] = NULL;
  }

  partial_lists = (KeyValueNode **)malloc(reduce_workers * sizeof(KeyValueNode *));
  for (i = 0; i < reduce_workers; i++) {
    partial_lists[i] = NULL;
  }
}

// Framework entry
void MR_Run(int argc, char *argv[], Mapper map, int num_mappers, Reducer reduce,
            int num_reducers, Combiner combine, Partitioner partition) {
  InitializeGlobalVariables(argc, argv, num_mappers, num_reducers, partition, map,
                            combine, reduce);

  AllocateMapHashTable();
  RunMapperThreadPool();

  RunReducerThreadPool();

  WaitForMappersToComplete();
  WaitForReducersToComplete();

  FreeMapResources();
  FreeReduceResources();

}

