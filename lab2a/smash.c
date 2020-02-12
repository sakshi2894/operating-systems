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
    char *path = "/bin";

    while (getline(&line, &len, f) != -1) {

        char* cmds = strsep(&line, ";");

        while (cmds != NULL) {

            char* cmd = strdup(cmds);

            if (strlen(cmd) > 0) {

                char *cmd_args_arr[500];
                char *cmd_args = strsep(&cmd, " ");
                int i = 0;
                while (cmd_args != NULL) {
                    if (strlen(cmd_args) > 0) {
                        //Handle trailing \n
                        size_t ln = strlen(cmd_args) - 1;
                        if (cmd_args[ln] == '\n') cmd_args[ln] = '\0';
                        
                        cmd_args_arr[i] = cmd_args;
                        i = i + 1;
                        cmd_args = strsep(&cmd, " ");
                    }
                }

                if (strcmp(cmd_args_arr[0], "exit") == 0) {
                    exit(0);
                    //printf("exit printed");
                } else if (strcmp(cmd_args_arr[0], "cd") == 0) {
                    printf("cd printed");
                } else if (strcmp(cmd_args_arr[0], "path") == 0) {
                    printf("path printed");
                } else {
                    printf("arg 0 is: %s", cmd_args_arr[0]);
                }

                /**
                char *cmd_args[len];
                //printf("str is at %p p is set at %p\n", str, p);

                while (p != NULL) {
                    if (strlen(p) > 0) {
                        printf("%s\n", p);
                    }

                    //Knows when to resume.
                    // Chnages str to store this context information.
                    // That's why doesn't work on constant strings.
                    //valgrind ./
                    p = strsep(&line, " ");
                }**/
            }

            cmds = strsep(&line, ";");
        }
        printf("smash>");

    }
    free(line);

}
