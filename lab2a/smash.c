//
// Created by Sakshi Bansal on 11/02/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zconf.h>
#include <ctype.h>


typedef struct Node {
    char* data;
    struct Node *next;
} Node;


void handle_error();
Node* init_path();
void set_cmd_args(char* cmd, int* cmd_size, char* cmd_args_arr[]);
void handle_path(char* cmd_args_arr[], Node **path_head);
void addToFront(Node **head, char *data);
int removeNode(Node **head, char *data);
void clear(Node **head);
void printList(Node *head);
int get_size(Node *head);
void run_parallel(Node *path_head, char* cmd);
int check_syntax(char* line);
int ARR_SIZE = 1000;

int main(int argc, char** argv) {

    FILE *f;
    int is_batch = 0;
    if (argc < 1) {
        handle_error();
        exit(1);
    } else if (argc == 1){
        f = stdin;
    } else {    //argc == 2
        char *filename = argv[1];
        f = fopen(filename, "r");
        if (f == NULL) {
            handle_error();
            exit(1);
        }
        is_batch = 1;
    }

    if (is_batch == 0) {
        printf("smash> ");
        fflush(stdout);
    }
    char *line = NULL;
    size_t len = 0;

    Node *path_head = init_path();

    while (getline(&line, &len, f) != -1) {

        if (check_syntax(strdup(line)) == -1) {
            handle_error();
            if (is_batch == 0) {
                printf("smash> ");
                fflush(stdout);
            }
            continue;
        }

        char* cmds = strsep(&line, ";");

        while (cmds != NULL) {

            char* cmd = strdup(cmds);
            char* cmd_dup = strdup(cmd);

            if (strlen(cmd) > 0) {

                int cmd_size = 0;
                char *cmd_args_arr[1000];

                set_cmd_args(cmd, &cmd_size, cmd_args_arr);

                if (cmd_size > 0) {
                    //Built-in commands.
                    if (strcmp(cmd_args_arr[0], "exit") == 0) {
                        exit(0);
                    } else if (strcmp(cmd_args_arr[0], "cd") == 0) {
                        int ret = chdir(cmd_args_arr[1]);
                        if (ret != 0) {
                            handle_error();
                        }
                    } else if (strcmp(cmd_args_arr[0], "path") == 0) {
                        handle_path(cmd_args_arr, &path_head);
                    } else {        //Other commands.
                        // Look for commands in path directories.
                        //Break cmd on &
                        char* parallel_cmds = strsep(&cmd_dup, "&");

                        while (parallel_cmds != NULL) {
                            char* parallel_cmd = strdup(parallel_cmds);
                            run_parallel(path_head, parallel_cmd);
                            parallel_cmds = strsep(&cmd_dup, "&");
                        }
                        //Wait for ALL children to finish.
                        while (wait(NULL) > 0);
                    }

                }
            }

            cmds = strsep(&line, ";");
        }

        if (is_batch == 0) {
            printf("smash> ");
            fflush(stdout);
        }

    }
    free(line);

}

void handle_error() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

Node* init_path() {
    Node *head = (Node*) malloc(sizeof(Node));
    head -> data = "/bin";
    head -> next = NULL;
    return head;
}

void set_cmd_args(char* cmd, int* cmd_size, char* cmd_args_arr[]) {

    const char *delimiters = " \t\n\v\f\r";
    char *cmd_args = strsep(&cmd, delimiters);
    while (cmd_args != NULL) {
        if (strlen(cmd_args) > 0) {
            cmd_args_arr[*cmd_size] = cmd_args;
            *cmd_size = *cmd_size + 1;
        }
        cmd_args = strsep(&cmd, " \t\n\v\f\r");
    }

    // Clearning rest of the array.
    int i = *cmd_size;
    for (; i < 1000; i++) {
        cmd_args_arr[i] = '\0';
    }
}


void handle_path(char* cmd_args_arr[], Node **path_head) {

    char* path_cmd = cmd_args_arr[1];
    if (strcmp(path_cmd, "add") == 0) {
        addToFront(path_head, cmd_args_arr[2]);
        //printList(*path_head);
    } else if (strcmp(path_cmd, "remove") == 0) {
        if (removeNode(path_head, cmd_args_arr[2]) == -1) {
            handle_error();
        }
        //printList(*path_head);
    } else if (strcmp(path_cmd, "clear") == 0) {
        clear(path_head);
        //printList(*path_head);
    }
}



void run_parallel(Node *path_head, char* cmd) {

    int cmd_size = 0;
    char* cmd_args_arr[1000];

    int i = 0;
    int count = 0;
    while (i < strlen(cmd)) {
        if(cmd[i] == '>') {
            count++;
        }
        i++;
    }
    int newfd = -1;
    int prev_stdout = dup(1);
    int prev_stderr = dup(2);

    int is_redirect = 1;
    if (count == 0) {
        is_redirect = 0;
    } else {
        char *pcmd = strdup(cmd);
        char *left = strsep(&pcmd, ">");
        char *right = strsep(&pcmd, ">");
        int rp = 0;
        char* file_name_args[2];
        set_cmd_args(right, &rp, file_name_args);
        FILE* file = fopen(file_name_args[0], "w");
        newfd = fileno(file);
        dup2(newfd, 1);
        dup2(newfd, 2);
        cmd = strdup(left);
    }

    set_cmd_args(cmd, &cmd_size, cmd_args_arr);

    if (cmd_size > 0) {
        Node *pNode = path_head;
        int cmd_run = 0;
        while (pNode != NULL) {
            char *path_dir = pNode->data;
            char *cmd_in_arg = strdup(cmd_args_arr[0]);
            char *full_path = (char *) malloc(2 + strlen(path_dir) + strlen(cmd_in_arg));

            strcat(full_path, path_dir);
            strcat(full_path, "/");
            strcat(full_path, cmd_in_arg);
            //printf("Searching for binary in path: %s\n", full_path);

            if (access(full_path, X_OK) == 0) {
                cmd_run = 1;
                int rc = fork();
                if (rc == 0) {
                    execv(full_path, cmd_args_arr);
                    handle_error();
                    exit(0);
                }
                break;
            }
            pNode = pNode->next;
        }

        if (is_redirect == 1) {
            dup2(prev_stdout, 1);
            dup2(prev_stderr, 2);
            close(prev_stdout);
            close(prev_stderr);
        }

        if (cmd_run == 0) {
            handle_error();
        }
    }



}

int check_syntax(char* line) {
    char* cmds = strsep(&line, ";");
    while (cmds != NULL) {
        char* single_cmd = strdup(cmds);
        char* parallel_cmd = strdup(cmds);
        int cmd_size = 0;
        char* cmd_args_arr[1000];
        set_cmd_args(single_cmd, &cmd_size, cmd_args_arr);
        if (cmd_size == 0) return 0;
        char* cmd = cmd_args_arr[0];

        if (strcmp(cmd, "exit") == 0) {
            if (cmd_size != 1) {
                return -1;
            }
        } else if (strcmp(cmd, "cd") == 0) {
            if (cmd_size == 1 || cmd_size > 2) {
                return -1;
            }
        } else if (strcmp(cmd, "path") == 0) {
            if (cmd_size < 2) {
                return -1;
            }
            char* path_cmd = cmd_args_arr[1];
            if (strcmp(path_cmd, "add") == 0) {
                if (cmd_size != 3) return -1;
            } else if (strcmp(path_cmd, "remove") == 0) {
                if (cmd_size != 3) return -1;
            } else if (strcmp(path_cmd, "clear") == 0) {
                if (cmd_size != 2) return -1;
            } else {
                return -1;
            }
        } else {    // Check for parallel and redirect commands.
            char* pcmd = strsep(&parallel_cmd, "&");
            while (pcmd != NULL) {
                char* single_pcmd = strdup(pcmd);
                //Check syntax for redirection.
                int i = 0;
                int count = 0;
                while (single_pcmd[i] != '\0') {
                    if(single_pcmd[i] == '>') {
                        count++;
                    }
                    i++;
                }

                if (count >= 2) return -1;
                if (count != 0) {
                    // Else check number of characters on the left and right of >
                    char *left = strsep(&pcmd, ">");
                    char *right = strsep(&pcmd, ">");

                    if (strcmp(left, "\0") == 0 || strcmp(right, "\0") == 0) return -1;
                    char *right_dup = strdup(right);
                    char *left_dup = strdup(left);
                    int psize = 0;

                    char* cmd_args_right[1000];
                    set_cmd_args(right_dup, &psize, cmd_args_right);
                    if (psize != 1) return -1;

                    psize = 0;
                    char* cmd_args_left[1000];
                    set_cmd_args(left_dup, &psize, cmd_args_left);
                    if (psize == 0) return -1;
                }

                pcmd = strsep(&parallel_cmd, "&");
            }

        }

        cmds = strsep(&line, ";");
    }

    return 0;

}

//Linked list methods below

void addToFront(Node **head, char *data) {

    // Create node
    Node *node = (Node*) malloc(sizeof(Node));
    node->data = data;
    node->next = *head;

    // Update head.
    (*head) = node;
}

int removeNode(Node **head, char *data) {
    int removed = -1;
    Node *temp = *head;
    Node *prev = NULL;
    while (temp != NULL) {
        if (strcmp(temp->data, data) == 0) { //This node has to be removed.

            removed = 1;
            //Removing first node.
            if (prev == NULL) {
                (*head) = temp-> next;
                free(temp);
                temp = *head;
            } else {
                prev -> next = temp -> next;
                free(temp);
                temp = prev -> next;
            }

        } else {
            prev = temp;
            temp = temp->next;
        }
    }
    return removed;
}


void clear(Node **head) {
    Node *temp = *head;
    while (temp != NULL) {
        Node *next = temp -> next;
        free(temp);
        temp = next;
    }
    *head = NULL;
}

void printList(Node *head) {
    Node *temp = head;
    while (temp != NULL) {
        printf("Data: %s\n", temp->data);
        temp = temp -> next;
    }
}

int get_size(Node *head) {
    int size = 0;
    Node *temp = head;
    while (temp != NULL) {
        size++;
        temp = temp -> next;
    }
    return size;
}
