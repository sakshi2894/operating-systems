#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mapreduce.h"

void Map(char *file_name) {
  FILE *fp = fopen(file_name, "r");
  assert(fp != NULL);

  char *line = NULL;
  size_t size = 0;
  while (getline(&line, &size, fp) != -1) {
    char *token, *dummy = line;
    while ((token = strsep(&dummy, " \t\n\r")) != NULL) {
      MR_EmitToCombiner(token, "1");
    }
  }
  free(line);
  fclose(fp);
}

void Combine(char *key, CombineGetter get_next) {
  //printf("Combine called for %s\n", key);
  int count = 0;
  char *value;
  while ((value = get_next(key)) != NULL) {
    count++;  // Emmited Map values are "1"s
  }
  // Convert integer (count) to string (value)
  value = (char *)malloc(10 * sizeof(char));
  sprintf(value, "%d", count);

  MR_EmitToReducer(key, value);
  free(value);
}

void Reduce(char *key, ReduceStateGetter get_state,
            ReduceGetter get_next, int partition_number) {
  char *state = get_state(key, partition_number);
  int count = (state != NULL) ? atoi(state) : 0;

  char *value = get_next(key, partition_number);
  if (value != NULL) {
    count += atoi(value);
    //printf("Count for %s pumped to %d with value %s\n", key, count, value);
    // Convert integer (count) to string (value)
    value = (char *)malloc(10 * sizeof(char));
    sprintf(value, "%d", count);

    MR_EmitReducerState(key, value, partition_number);
    free(value);
  } else {
    printf("%s %d\n", key, count);
  }
}
int main(int argc, char *argv[]) {
  MR_Run(argc, argv, Map, 10,
         Reduce, 10, Combine, MR_DefaultHashPartition);
}
