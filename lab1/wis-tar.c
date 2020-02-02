//
// Created by Sakshi Bansal on 24/01/20.
//

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <sys/stat.h>

void write_file_content(FILE *f, FILE *tf);
int main(int argc, char** argv) {
    // First element in argv is the program name
    if (argc < 3) {
        printf ("wis-tar: tar-file file […]\n");
        exit(1);
    }

    char *tarname = argv[1];
    FILE* tf = fopen(tarname, "w");

    int i;
    for (i = 2; i < argc; i++) {
        char *filename = argv[i];
        FILE *f = fopen(filename, "r");
        if (f == NULL) {
            printf("wis-tar: cannot open file\n");
            exit(1);
        }

        // Get file name
        char filenamewrite[101];
        strncpy(filenamewrite, filename, 100);
        filenamewrite[100] = '\0';

        long filename_length = strlen(filename);

        //Write everything out.
        fprintf(tf, "%s", filenamewrite);
        long j;
        for (j = filename_length; j < 100; j++) {
            char null_character = '\0';
            fwrite(&null_character, sizeof(char), 1, tf);
        }

        // get size of file.
        struct stat info;
        int err = stat(filename, &info);
        //Error checking required here.
        if (err == -1) {
            printf("wis-tar: cannot open file\n");
            exit(1);
        }

        long filesize = info.st_size;

        fwrite(&filesize, sizeof(long), 1, tf);

        //Write file content
        write_file_content(f, tf);
        fclose(f);
    }

    fclose(tf);
    return 0;
}


void write_file_content(FILE *f, FILE *tf) {
    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, f) != -1) {
        fprintf(tf, "%s", line);
    }
    free(line);
}
