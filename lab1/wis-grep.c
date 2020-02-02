#include<stdio.h>
#include<stdlib.h>
#include <string.h>

void grep_function(char * search_term, FILE *f);

int main(int argc, char** argv) {
    // First element in argv is the program name
    if (argc < 2) {
        printf("wis-grep: searchterm [file …]\n");
        exit(1);
    }
    char *search_term = argv[1];

    int i;
    if (argc > 2) {
        for (i = 2; i < argc; i++) {
            char *filename = argv[i];
            FILE *f = fopen(filename, "r");
            if (f == NULL) {
                printf("wis-grep: cannot open file\n");
                exit(1);
            }
            grep_function(search_term, f);
            fclose(f);
        }
    } else { // Read from stdin.
        grep_function(search_term, stdin);
    }
    return 0;
}

void grep_function(char *search_term, FILE *f) {
    char *read_buffer = NULL;
    size_t read_buffer_size;

    ssize_t size = getline(&read_buffer, &read_buffer_size, f);

    while (size != -1) {
        if (strstr(read_buffer, search_term) != NULL) {
            printf("%s", read_buffer);
        }
        size = getline(&read_buffer, &read_buffer_size, f);
    }

}

