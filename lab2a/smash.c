//
// Created by Sakshi Bansal on 11/02/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zconf.h>

int main(int argc, char** argv) {

    char error_message[30] = "An error has occurred\n";
    FILE *f;
    int is_batch = 0;
    if (argc < 1) {
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    } else if (argc == 1){
        f = stdin;
    } else {    //argc == 2
        char *filename = argv[1];
        FILE *f = fopen(filename, "r");
        if (f == NULL) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        is_batch = 1;
    }

    if (is_batch == 0) {
        printf("smash>");
    }
    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, f) != -1) {
        //fprintf(tf, "%s", line);
        if (strcmp(line, "exit") == 0) {
            return 0;
        }
        printf("smash>");

    }
    free(line);

}
