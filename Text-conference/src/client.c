#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> // Needed for memset
#include <netinet/ip.h> /* superset of previous */
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "common.h"
#include "api.h"

/**
 * @file
 * 
 */

/**
 * Data structures and constants
 */
#define MAX_USERINPUT (MAX_DATA + 50)

enum Command {
    COM_LOGIN,
    COM_LOGOUT,
    COM_JOINSESSION,
    COM_LEAVESESSION,
    COM_CREATESESSION,
    COM_LIST,
    COM_QUIT,
    COM_TEXT, 
    COM_WRONG
};

/**
 * Main variables
 */

static int sockfd;

static struct sockaddr_in servaddr;

static int initsocket();

static unsigned char clientId[MAX_NAME] = "test";

typedef struct UserManageStruct  {
    bool loggedin;
    bool connected;
} UserManageStruct;

UserManageStruct thisClient = {.loggedin = false, .connected=false};

int parse_command(const char *cmd_line, int *argc, char ***argv_ptr);
void logic(const char *cmd_line);
enum Command getCommand(const char*cmd_line);

int main(int argc, char *argv[]) {
    int retval;

    initsocket();

    while (true) {
        // Watch stdin (fd 0) and sockfd to see when it has input

        // user not logged in, there shouldn't be connection and sockfd
        // otherwise, the user can leave and join another session 


        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        int maxfd = STDIN_FILENO;

        // if (thisClient.connected) {
        //     FD_SET(sockfd, &readfds);
        //     maxfd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;
        // }
        maxfd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

        retval = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (retval < 0) {
            printf("select() error\n");
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char input[MAX_DATA];
            struct Message message;

            if (fgets(input, sizeof(input), stdin) != NULL) {
                enum Command command = getCommand(input);

                switch (command)
                {
                case COM_LOGIN:
                    logic(input);
                    break;
                case COM_LOGOUT:
                    logic(input);
                    break;
                case COM_JOINSESSION:
                    logic(input);
                    break;
                case COM_LEAVESESSION:
                    logic(input);
                    break;
                case COM_CREATESESSION:
                    logic(input);
                    break;
                case COM_LIST:
                    logic(input);
                    break;
                case COM_QUIT:
                    logic(input);
                    break;
                case COM_TEXT:
                    // build struct Message
                    char *buf;
                    int size;
                    message.type = COM_TEXT;
                    memcpy((char *) message.source, "123", 4);
                    strncpy((char *) message.data, input, MAX_DATA - 1);
                    message.data[MAX_DATA-1] = '\0';

                    buf = serialize(message, &size);

                    ssize_t nbytes = send(sockfd, buf, strlen(buf) * sizeof(char), 0);
                    if (nbytes < 0) {
                        perror("send failed");
                    }
                    break;
                case COM_WRONG:
                    break;
                default:
                    printf("Unknown command\n");
                    break;
                }
            }

            
        }

        if (FD_ISSET(sockfd, &readfds)) {
            char buffer[1024];
            ssize_t nbytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
            if (nbytes < 0) {
                perror("recv error");
            } else if (nbytes == 0) {
                printf("server closed\n");
                break;
            } else {
                buffer[nbytes] = '\0';
                printf("%s", buffer);
            }
        }
        
    }
    

    close(sockfd);
    return 0;
}

int initsocket() {
    char destaddr[] = "127.0.0.1";
    int destport = 2345;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(1);
    }

    //Socket setup

    struct in_addr inp;
    if (inet_aton(destaddr, &inp) == 0 ) {
        printf("ipv4 addr incorrect\n");
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(destport);  
    servaddr.sin_addr = inp;  

    if ( connect(sockfd, (struct sockaddr *)&servaddr,
                   sizeof servaddr) == -1 ) {

        printf("sock conn error\n");
        close(sockfd);
        exit(1);    // Or return -1 if you want to handle it in main()

    }

    // char buf[] = "message";
    // ssize_t size = sizeof(buf) / sizeof(char);
    // ssize_t nbytes = send(sockfd, buf, size, 0);
    // printf("%ld bytes sent\n", nbytes);
}

int initiomultiplex() {
    
}
 
int parse_command(const char *cmd_line, int *argc, char ***argv_ptr) {
    if (!cmd_line || !argc || !argv_ptr) {
        return -1;
    }

    // First pass: count arguments
    *argc = 0;
    int in_word = 0;
    int in_quotes = 0;
    const char *p = cmd_line;

    while (*p) {
        if (*p == '"') {
            in_quotes = !in_quotes;
            if (!in_word) {
                (*argc)++;
                in_word = 1;
            }
        } else if (isspace(*p) && !in_quotes) {
            in_word = 0;
        } else if (!in_word) {
            (*argc)++;
            in_word = 1;
        }
        p++;
    }

    // Allocate argv array
    *argv_ptr = malloc((*argc + 1) * sizeof(char *));
    if (!*argv_ptr) {
        return -1;
    }

    // Second pass: copy arguments
    char **argv = *argv_ptr;
    int arg_idx = 0;
    in_word = 0;
    in_quotes = 0;
    p = cmd_line;
    const char *word_start = NULL;
    
    while (1) {
        if (*p == '"' || *p == '\0' || (isspace(*p) && !in_quotes)) {
            if (in_word) {
                // End of current word
                int len = p - word_start;
                if (in_quotes && *p == '"') {
                    len = p - word_start;
                }
                
                argv[arg_idx] = malloc(len + 1);
                if (!argv[arg_idx]) {
                    // Cleanup on failure
                    for (int i = 0; i < arg_idx; i++) {
                        free(argv[i]);
                    }
                    free(argv);
                    return -1;
                }
                
                strncpy(argv[arg_idx], word_start, len);
                argv[arg_idx][len] = '\0';
                arg_idx++;
                in_word = 0;
            }
            
            if (*p == '"') {
                in_quotes = !in_quotes;
                if (!in_word) {
                    word_start = p + 1;
                    in_word = 1;
                }
            }
        } else if (!in_word) {
            word_start = p;
            in_word = 1;
        }
        
        if (*p == '\0') break;
        p++;
    }

    argv[*argc] = NULL;  // NULL terminate the array
    return 0;
}


void logic(const char *cmd_line) {
    int argc;
    char **argv;

    if (parse_command(cmd_line, &argc, &argv) == 0) {

        if (strcmp(argv[0], "/login") == 0) {
            printf("%d\n", argc);
            if (argc != 5) {
                printf("Usage: /login <client ID> <password> <server-IP> <server-port>\n");
            }
        } else if (strcmp(argv[0], "/logout") == 0) {
            printf("Command: LOGOUT\n");
        } else if (strcmp(argv[0], "/joinsession") == 0) {
            if (argc != 2) {
                printf("Usage: /joinsession <session ID>\n");
            }
        } else if (strcmp(argv[0], "/leavesession") == 0) {
        } else if (strcmp(argv[0], "/createsession") == 0) {
            if (argc != 2) {
                printf("Usage: /createsession <session ID>\n");
            }
        } else if (strcmp(argv[0], "/list") == 0) {
            printf("Command: LIST\n");
        } else if (strcmp(argv[0], "/quit") == 0) {
            printf("Command: QUIT\n");
        } else {
            printf("<<<<<Log in before talking>>>>>\n");
        }
        
        // Cleanup
        for (int j = 0; j < argc; j++) {
            free(argv[j]);
        }
        free(argv);
    }
}

enum Command getCommand(const char*cmd_line){
    int argc;
    char **argv;
    enum Command retval = COM_WRONG;

    if (parse_command(cmd_line, &argc, &argv) == 0)  {
        if (strcmp(argv[0], "/login") == 0) {
            retval = COM_LOGIN;
        } else if (strcmp(argv[0], "/logout") == 0) {
            retval = COM_LOGOUT;
        } else if (strcmp(argv[0], "/joinsession") == 0) {
            retval = COM_JOINSESSION;
        } else if (strcmp(argv[0], "/leavesession") == 0) {
            retval = COM_LEAVESESSION;
        } else if (strcmp(argv[0], "/createsession") == 0) {
            retval = COM_CREATESESSION;
        } else if (strcmp(argv[0], "/list") == 0) {
            retval = COM_LIST;
        } else if (strcmp(argv[0], "/quit") == 0) {
            retval = COM_QUIT;
        } else {
            retval = COM_TEXT;
        }
        
        // Cleanup
        for (int j = 0; j < argc; j++) {
            free(argv[j]);
        }
        free(argv);
    }

    return retval;
    
}