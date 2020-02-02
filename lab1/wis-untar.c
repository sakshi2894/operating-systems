//
// Created by Sakshi Bansal on 26/01/20.
//

#include<stdio.h>
#include<stdlib.h>


void write_file_content(FILE *f, FILE *tf, long fsize);

int main(int argc, char** argv) {
    // First element in argv is the program name
    if (argc < 2 || argc > 2) {
        printf("wis-untar: tar-file\n");
        exit(1);
    }

    char* tfilename = argv[1];
    FILE* tf = fopen(tfilename, "r");
    if (tf == NULL) {
        printf("wis-untar: cannot open file\n");
        exit(1);
    }


    char fname[101];

    while (fgets(fname, 101, tf) != NULL) {
        FILE *f = fopen(fname, "w");

        //Read 8 bytes for file size;
        long fsize = 0;
        fread(&fsize, sizeof(long), 1, tf);

        //Read content and write it out to file
        write_file_content(f, tf, fsize);

        //Close file.
        fclose(f);
    }

    fclose(tf);
    return 0;
}

void write_file_content(FILE *f, FILE *tf, long fsize) {
    int i;
    for (i = 0; i < fsize; i++) {
        char c = fgetc(tf);
        fputc(c, f);
    }
}
