//
// Created by Sakshi Bansal on 11/02/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zconf.h>



typedef struct Node {
    char* data;
    struct Node *next;
} Node;


void addToFront(Node **head, char *data) {

    // Create node
    Node *node = (Node*) malloc(sizeof(Node));
    node->data = data;
    node->next = *head;

    // Update head.
    (*head) = node;
}

void removeNode(Node **head, char *data) {
    Node *temp = *head;
    Node *prev = NULL;
    while (temp != NULL) {
        if (strcmp(temp->data, data) == 0) { //This node has to be removed.

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

void handle_error();
Node* init_path();

int main(int argc, char** argv) {

    FILE *f;
    int is_batch = 0;
    if (argc < 1) {
        handle_error();
    } else if (argc == 1){
        f = stdin;
    } else {    //argc == 2
        char *filename = argv[1];
        FILE *f = fopen(filename, "r");
        if (f == NULL) {
            handle_error();
        }
        is_batch = 1;
    }

    if (is_batch == 0) {
        printf("smash>");
    }
    char *line = NULL;
    size_t len = 0;

    Node *path_head = init_path();

    while (getline(&line, &len, f) != -1) {

        char* cmds = strsep(&line, ";");

        while (cmds != NULL) {

            char* cmd = strdup(cmds);

            if (strlen(cmd) > 0) {

                char *cmd_args_arr[500];
                char *cmd_args = strsep(&cmd, " ");
                int cmd_size = 0;
                while (cmd_args != NULL) {
                    if (strlen(cmd_args) > 0) {
                        //Handle trailing \n
                        size_t ln = strlen(cmd_args) - 1;
                        if (cmd_args[ln] == '\n') cmd_args[ln] = '\0';

                        cmd_args_arr[cmd_size] = cmd_args;
                        cmd_size = cmd_size + 1;
                    }
                    cmd_args = strsep(&cmd, " ");
                }

                if (strcmp(cmd_args_arr[0], "exit") == 0) {
                    exit(0);
                } else if (strcmp(cmd_args_arr[0], "cd") == 0) {
                    if (cmd_size == 1 || cmd_size > 2 ) {
                        handle_error();
                    }
                    chdir(cmd_args_arr[1]);
                } else if (strcmp(cmd_args_arr[0], "path") == 0) {
                    if (cmd_size < 2) {
                        handle_error();
                    }
                    char* path_cmd = cmd_args_arr[1];
                    if (strcmp(path_cmd, "add") == 0) {
                        if (cmd_size != 3) handle_error();
                        addToFront(&path_head, cmd_args_arr[2]);
                        printList(path_head);
                    } else if (strcmp(path_cmd, "remove") == 0) {
                        if (cmd_size != 3) handle_error();
                        removeNode(&path_head, cmd_args_arr[2]);
                        printList(path_head);
                    } else if (strcmp(path_cmd, "clear") == 0) {
                        if (cmd_size != 2) handle_error();
                        clear(&path_head);
                        printList(path_head);
                    } else {
                        handle_error();
                    }
                   // printf("path printed");
                } else {
                    //printf("arg 0 is: %s", cmd_args_arr[0]);
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

        if (is_batch == 0) {
            printf("smash>");
        }

    }
    free(line);

}

void handle_error() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(1);
}

Node* init_path() {
    Node *head = (Node*) malloc(sizeof(Node));
    head -> data = "/bin";
    head -> next = NULL;
    return head;
}
