#include "mapreduce.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

#define NUM_KEY_LISTS 3001

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

// Structure representing a node in Key Value list
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
KeyNode ***combine_ds;
int mappers_done;
Mapper map_fun;
Combiner combine_fun;
ValueNode *valueNodeToFree;

//Reduce state variables
pthread_mutex_t *buffer_locks;
sem_t *sem_filled_locks;
Partitioner partition_fun;
int reduce_workers;
pthread_t *reduce_threads;
KeyValueNode **reduce_buffer;
KeyValueNode ***reduce_partial;
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

//Utility function to make a copy of char array
char *CopyStr(char *data) {
  char *copy = (char *)malloc((strlen(data) + 1) * sizeof(char));  //Freed
  strcpy(copy, data);
  return copy;
}

// Allocates a new Key Node with given key
KeyNode *AllocateKeyNode(char *key) {
  KeyNode *key_node = (KeyNode *)malloc(sizeof(KeyNode));
  key_node->key = CopyStr(key);
  key_node->value_list_head = NULL;
  key_node->next_key = NULL;
  return key_node;
}

// Allocates a new Value Node with given value
ValueNode *AllocateValueNode(char *value) {
  ValueNode *value_node = (ValueNode *)malloc(sizeof(ValueNode));
  value_node->value = CopyStr(value);
  value_node->next_value = NULL;
  return value_node;
}

// Allocates a new KeyValue Node with given key, value
KeyValueNode *AllocateKeyValueNode(char *key, char *value) {
  KeyValueNode *key_value_node = (KeyValueNode *)malloc(sizeof(KeyValueNode));
  key_value_node->key = CopyStr(key);
  key_value_node->value = CopyStr(value);
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

// Stores the lists of strings which can only be cleared at the end.
void FreeStringEnd(char *data) {
  ValueNode *node = (ValueNode *)malloc(sizeof(ValueNode));  //Freed
  node->value = data;
  node->next_value = NULL;

  if (valueNodeToFree == NULL) {
    valueNodeToFree = node;
  } else {
    node->next_value = valueNodeToFree;
    valueNodeToFree = node;
  }
}

// Initializing the Hash Table for each Map thread
void AllocateMapHashTable() {
  combine_ds = (KeyNode ***)malloc(map_workers * sizeof(KeyNode **));  //Freed
  int i, j;
  for (i = 0; i < map_workers; i++) {
    combine_ds[i] = (KeyNode **)malloc(NUM_KEY_LISTS * sizeof(KeyNode *));  //Freed
    for (j = 0; j < NUM_KEY_LISTS; j++) {
      combine_ds[i][j] = NULL;
    }
  }
}

// Initializing the Hash Table for each Reduce thread
void AllocateReduceHashTable() {
  reduce_partial = (KeyValueNode ***)malloc(reduce_workers * sizeof(KeyValueNode **));  //Freed
  int i, j;
  for (i = 0; i < reduce_workers; i++) {
    reduce_partial[i] = (KeyValueNode **)malloc(NUM_KEY_LISTS * sizeof(KeyValueNode *));  //Freed
    for (j = 0; j < NUM_KEY_LISTS; j++) {
      reduce_partial[i][j] = NULL;
    }
  }
}

// Freeing up memory used by Hash Table of each Map thread
void FreeMapHashTable() {
  int i;
  for (i = 0; i < map_workers; i++) {
    free(combine_ds[i]);
  }
  free(combine_ds);
}

// Freeing up memory used by Hash Table of each Reduce thread
void FreeReduceHashTable() {
  int i;
  for (i = 0; i < reduce_workers; i++) {
    free(reduce_partial[i]);
  }
  free(reduce_partial);
}

// Adding key and value to the hash table
void MR_EmitToCombiner(char *key, char *value) {
  int worker_idx = FindMapperThreadIndex();
  int hash_idx = MR_DefaultHashPartition(key, NUM_KEY_LISTS);

  KeyNode *key_node = FindKeyNode(combine_ds[worker_idx][hash_idx], key);
  if (key_node == NULL) {
    // Inserting new key at front
    key_node = AllocateKeyNode(key);  //Freed
    key_node->next_key = combine_ds[worker_idx][hash_idx];
    combine_ds[worker_idx][hash_idx] = key_node;
  }
  // Inserting value count at front
  ValueNode *value_node = AllocateValueNode(value);  //Freed
  value_node->next_value = key_node->value_list_head;
  key_node->value_list_head = value_node;
}

// getnext function for Combine
char *CombineIterator(char *key) {
  int worker_idx = FindMapperThreadIndex();
  int hash_idx = MR_DefaultHashPartition(key, NUM_KEY_LISTS);
  //printf("# %d %d\n", worker_idx, hash_idx);
  KeyNode *key_node = FindKeyNode(combine_ds[worker_idx][hash_idx], key);
  if (key_node == NULL)
    return NULL;
  ValueNode *value_list_head = key_node->value_list_head;
  if (value_list_head == NULL)
    return NULL;
  ValueNode *next_value = value_list_head->next_value;
  char *value = value_list_head->value;
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
    while ((key_head = combine_ds[worker_idx][i]) != NULL) {
      KeyNode *next_key = key_head->next_key;
      combine_fun(key_head->key, &CombineIterator);
      free(key_head->key);
      free(key_head);
      combine_ds[worker_idx][i] = next_key;
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
      break;
    }
    map_fun(filenames[idx]);
    if (combine_fun != NULL) {
      IterateKeysToCombine();
    }
  }
  return NULL;
}

// Adds to reduce_buffer DS
void MR_EmitToReducer(char *key, char *value) {
  unsigned long reduce_hash_idx = partition_fun(key, reduce_workers);

  KeyValueNode *key_value_node = AllocateKeyValueNode(key, value);  // Freed
  pthread_mutex_lock(&buffer_locks[reduce_hash_idx]);

  // Produce
  key_value_node->next = reduce_buffer[reduce_hash_idx];
  reduce_buffer[reduce_hash_idx] = key_value_node;

  sem_post(&sem_filled_locks[reduce_hash_idx]);
  pthread_mutex_unlock(&buffer_locks[reduce_hash_idx]);
}

// Adds to reduce_partial DS
void MR_EmitReducerState(char *key, char *state, int partition_number) {
  int hash_idx = MR_DefaultHashPartition(key, NUM_KEY_LISTS);
  KeyValueNode *keyValueNode = FindKeyValueNode(reduce_partial[partition_number][hash_idx], key);
  if (keyValueNode != NULL) {
    // Update it with new value.
    free(keyValueNode->value);
    keyValueNode->value = CopyStr(state);
  } else {
    // Else add a new node with given value.
    KeyValueNode *newNode = AllocateKeyValueNode(key, state);  // Freed
    newNode->next = reduce_partial[partition_number][hash_idx];
    reduce_partial[partition_number][hash_idx] = newNode;
  }
}

// get_state() for Reducer
char *ReduceStateIterator(char *key, int partition_number) {
  // Find the key in the data structure.
  int hash_idx = MR_DefaultHashPartition(key, NUM_KEY_LISTS);
  KeyValueNode *keyValueNode = FindKeyValueNode(reduce_partial[partition_number][hash_idx], key);
  if (keyValueNode == NULL)
    return NULL;

  return keyValueNode->value;
}

// get_next() for Reducer
char *ReduceIterator(char *key, int partition_number) {
  char *value = NULL;
  pthread_mutex_lock(&buffer_locks[partition_number]);
  if (mappers_done != 1 || reduce_buffer[partition_number] != NULL) {
    KeyValueNode *keyValueNode = reduce_buffer[partition_number];
    KeyValueNode *prev = NULL;

    while (keyValueNode != NULL) {
      if (strcmp(keyValueNode->key, key) == 0) {
        if (prev == NULL) {  //This is the first node.
          reduce_buffer[partition_number] = keyValueNode->next;
        } else {
          prev->next = keyValueNode->next;
        }
        value = keyValueNode->value;

        // Free resources
        free(keyValueNode->key);
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

// For each key, call Reduce
void *ReduceWrapper(void *args) {
  int idx = FindReducerThreadIndex();

  while (1) {
    sem_wait(&sem_filled_locks[idx]);
    if (mappers_done == 1 && reduce_buffer[idx] == NULL) {
      //Run reduce for all the keys of this partition
      int i;
      for (i = 0; i < NUM_KEY_LISTS; i++) {
        KeyValueNode *key_value_head;
        while ((key_value_head = reduce_partial[idx][i]) != NULL) {
          KeyValueNode *next_key_value = key_value_head->next;
          reduce_fun(key_value_head->key, &ReduceStateIterator, &ReduceIterator, idx);
          reduce_partial[idx][i] = next_key_value;

          // Free resources
          free(key_value_head->key);
          free(key_value_head->value);
          free(key_value_head);
        }
      }

      break;
    }

    pthread_mutex_lock(&buffer_locks[idx]);
    char *key = CopyStr(reduce_buffer[idx]->key);  //Freed
    pthread_mutex_unlock(&buffer_locks[idx]);

    reduce_fun(key, &ReduceStateIterator, &ReduceIterator, idx);
    free(key);
  }

  return NULL;
}

// Freeing resources used my Mappers
void FreeMapResources() {
  free(map_threads);
  FreeMapHashTable();
  free(filenames);
}

// Freeing resources used my Reducers
void FreeReduceResources() {
  free(reduce_threads);
  free(sem_filled_locks);
  free(buffer_locks);
  free(reduce_buffer);
  FreeReduceHashTable();
}

// Finally Freeing the value nodes accumulated in valueNodeToFree list
void FreeAllValueNodes() {
  while (valueNodeToFree != NULL) {
    ValueNode *next = valueNodeToFree->next_value;
    free(valueNodeToFree->value);
    free(valueNodeToFree);
    valueNodeToFree = next;
  }
}

// Creating workers to run Reducers
void RunReducerThreadPool() {
  reduce_threads = (pthread_t *)malloc(reduce_workers * sizeof(pthread_t));  //Freed
  int i;
  for (i = 0; i < reduce_workers; i++) {
    pthread_create(&reduce_threads[i], NULL, ReduceWrapper, NULL);
  }
}

// Creating workers to run Mappers
void RunMapperThreadPool() {
  pthread_mutex_init(&f_index_lock, NULL);
  map_threads = (pthread_t *)malloc(map_workers * sizeof(pthread_t));  //Freed
  int i;
  for (i = 0; i < map_workers; i++) {
    pthread_create(&map_threads[i], NULL, MapWrapper, NULL);
  }
}

//Waits for Mappers to finish executing
void WaitForMappersToComplete() {
  int i;
  for (i = 0; i < map_workers; i++) {
    pthread_join(map_threads[i], NULL);
  }
  mappers_done = 1;
}

// Wakes up reducers to terminate
void WaitForReducersToComplete() {
  int i;
  for (i = 0; i < reduce_workers; i++) {
    sem_post(&sem_filled_locks[i]);
    pthread_join(reduce_threads[i], NULL);
  }
}

// Populates the filenames in global array
void InitFileNames(char *argv[]) {
  filenames = (char **)malloc(num_files * sizeof(char *));  //Freed
  int i;
  for (i = 0; i < num_files; i++) {
    filenames[i] = argv[i + 1];
  }
}

// Initializes data structures for Reduce
void InitReduceDS() {
  int i;
  reduce_buffer = (KeyValueNode **)malloc(reduce_workers * sizeof(KeyValueNode *));
  for (i = 0; i < reduce_workers; i++) {
    reduce_buffer[i] = NULL;
  }
  AllocateReduceHashTable();
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

  InitFileNames(argv);

  int i;
  buffer_locks = (pthread_mutex_t *)malloc(reduce_workers * sizeof(pthread_mutex_t));  // Freed
  sem_filled_locks = (sem_t *)malloc(reduce_workers * sizeof(sem_t));                  // Freed
  for (i = 0; i < reduce_workers; i++) {
    pthread_mutex_init(&buffer_locks[i], NULL);
    sem_init(&sem_filled_locks[i], 0, 0);
  }

  InitReduceDS();
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
  FreeAllValueNodes();
}
