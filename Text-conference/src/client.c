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
    COM_WRONG, // free() needed
    COM_ERROR, // no malloc() succeeded -> no free() needed in parse_command()
};

/**
 * Main variables
 */

static int sockfd;

static struct sockaddr_in servaddr;

static int initsocket();

static unsigned char clientId[MAX_NAME] = "test";

typedef struct UserManageStruct  {
    bool connected;
} UserManageStruct;

UserManageStruct thisClient = { .connected=false};

enum Command parse_command(const char *cmd_line, int *argc, char ***argv_ptr);
void routine_main();
enum Command getCommand(const char*cmd_line);
int routine_login();

int main(int argc, char *argv[]) {
    

    while (true) {
        // Watch stdin (fd 0) and sockfd to see when it has input

        // user not logged in, there shouldn't be connection and sockfd
        // otherwise, the user can leave and join another session

        if (!thisClient.connected) {
            printf("login mode\n");
            routine_login();
        }
        
        if (thisClient.connected) {
            printf("main mode\n");
            routine_main();
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

    printf("sockfd %d\n", sockfd);

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
        return -1;

    }

    return 0;
    // char buf[] = "message";
    // ssize_t size = sizeof(buf) / sizeof(char);
    // ssize_t nbytes = send(sockfd, buf, size, 0);
    // printf("%ld bytes sent\n", nbytes);
}

 
enum Command parse_command(const char *cmd_line, int *argc, char ***argv_ptr) {
    if (!cmd_line || !argc || !argv_ptr) {
        return COM_ERROR;
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
        return COM_ERROR;
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
                    return COM_ERROR;
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

    if (strcmp(argv[0], "/login") == 0) {
        if (*argc == 5) {
            return COM_LOGIN;
        }
        printf("Usage: /login <client ID> <password> <server-IP> <server-port>\n");
    } else if (strcmp(argv[0], "/logout") == 0) {
        printf("Command: LOGOUT\n");
        return COM_LOGOUT;
    } else if (strcmp(argv[0], "/joinsession") == 0) {
        if (*argc == 2) {
            return COM_JOINSESSION;
        }
        printf("Usage: /joinsession <session ID>\n");
    } else if (strcmp(argv[0], "/leavesession") == 0) {
        printf("leave session\n");
        return COM_LEAVESESSION;
    } else if (strcmp(argv[0], "/createsession") == 0) {
        if (*argc == 2) {
            return COM_CREATESESSION;
        }
        printf("Usage: /createsession <session ID>\n");
    } else if (strcmp(argv[0], "/list") == 0) {
        printf("Command: LIST\n");
        return COM_LIST;
    } else if (strcmp(argv[0], "/quit") == 0) {
        printf("Command: QUIT\n");
        return COM_QUIT;
    } else {
        //text
        return COM_TEXT;
    }

    return COM_WRONG;
}

void cleanargs(int argc, char **argv) {
    for (int j = 0; j < argc; j++) {
        free(argv[j]);
    }
    free(argv); 
}

void routine_stdin() {

    int argc;
    char **argv;

    enum Command command;
    char cmd_line[MAX_DATA * 2];
    if (fgets(cmd_line, sizeof(cmd_line), stdin) == NULL) {
        return;
    }
    command = parse_command(cmd_line, &argc, &argv);

    Message message;

    if (command == COM_WRONG) {
        cleanargs(argc, argv);
    } else if(command == COM_LOGIN) {
        printf("/login invalid (alr executed)\n");
        cleanargs(argc, argv);
    } else if(command == COM_LOGOUT) {
        printf("logging out....\n");
        close(sockfd);
        thisClient.connected = false;
        cleanargs(argc, argv);
    }else if(command == COM_CREATESESSION) {
        cleanargs(argc, argv);
    }else if(command == COM_JOINSESSION) {
        cleanargs(argc, argv);
    }else if(command == COM_LEAVESESSION) {
        cleanargs(argc, argv);
    }else if(command == COM_LIST) {
        cleanargs(argc, argv);
    }else if(command == COM_TEXT) {
        // build struct Message
        char *buf;
        int size;
        message.type = COM_TEXT;
        memcpy((char *) message.source, "123", 4);
        strncpy((char *) message.data, cmd_line, MAX_DATA - 1);
        message.data[MAX_DATA-1] = '\0';
        
        buf = serialize(message, &size);
        ssize_t nbytes = send(sockfd, buf, strlen(buf) * sizeof(char), 0);
        if (nbytes < 0) {
            perror("send failed");
        }
        free(buf);

        printf(">> From YOU!: %s", cmd_line);
        cleanargs(argc, argv);
    }else if(command == COM_QUIT) {
        cleanargs(argc, argv);
    }else if(command == COM_ERROR) {
        perror("system error (parse)\n");
        exit(1);
    } else {
        printf("input error (no option)\n");
    }
}

void routine_sockfd() {
    Message message;
    char buffer[1024];
    ssize_t nbytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (nbytes < 0) {
        perror("recv error");
    } else if (nbytes == 0) {
        printf("server closed\n");
        thisClient.connected = false;
    } else {
        buffer[nbytes] = '\0';
        deserialize(buffer, &message);
        printf(">> From chat: %s", message.data);
    }
}

void routine_main() {
    int retval;

    while (thisClient.connected)
    {
        /* code */
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        int maxfd = STDIN_FILENO;

        maxfd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

        retval = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (retval < 0) {
            printf("select() error\n");
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            routine_stdin();
        }

        if (FD_ISSET(sockfd, &readfds)) {
            routine_sockfd();
        }
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

int routine_login() {

    int argc;
    char **argv;
    char input[MAX_DATA * 2];
    char reply[MAX_DATA];
    Message reply_msg;
    Message message;
    message.type = MT_LOGIN;
    char* str;

    while( !thisClient.connected) {

        if (fgets(input, sizeof(input), stdin) != NULL) {
            if (parse_command(input, &argc, &argv) != COM_ERROR) {

                if (argc != 5 || strcmp(argv[0], "/login") != 0 ) {
                    printf("Login with: /login <client ID> <password> <server-IP> <server-port>\n");
                    for (int j = 0; j < argc; j++) {
                        free(argv[j]);
                    }
                    free(argv);
                    
                    continue;
                }

                // from here, enough args and verified the command is "/login"
                // attemp login, if fails, ask user to try again
                if (initsocket() == -1) {
                    printf("Try another addr\n");
                    continue;
                }

                
                // from here, socket is connected
                strcpy((char *) message.source, argv[1]);
                strcpy((char *)message.data, argv[2]);
                str = serialize(message, NULL);

                ssize_t nbytes =  send(sockfd, str, strlen(str)*sizeof(char), 0);
                if (nbytes < 0) {
                    printf("send failed");
                    free(str);
                    continue;
                }

                nbytes = recv(sockfd, reply, MAX_DATA, 0);
                deserialize(reply, &reply_msg);

                switch (reply_msg.type)
                {
                case MT_LO_ACK:
                    thisClient.connected = true;
                    printf("logged in & connected \n");
                    break;
                case MT_LO_NAK:
                    printf("wrong userid / password \n");
                    close(sockfd);
                    break;
                default:
                    printf("login protocol failed\n");
                    close(sockfd);
                    break;
                }

                free(str);

            }

        }
    }
}

