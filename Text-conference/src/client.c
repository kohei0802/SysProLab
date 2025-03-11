#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> // Needed for memset
#include <netinet/ip.h> /* superset of previous */
#include <unistd.h>
#include <arpa/inet.h>
#include "common.h"

/**
 * @file
 * 
 */

#define MAX_USERINPUT (MAX_DATA + 50)

static int sockfd;

static struct sockaddr_in servaddr;

static int initsocket();

static unsigned char clientId[MAX_NAME] = "test";

int main(int argc, char *argv[]) {
    int retval;

    initsocket();

    while (true) {
        // Watch stdin (fd 0) and sockfd to see when it has input
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int maxfd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

        retval = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (retval < 0) {
            printf("select() error\n");
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char input[MAX_DATA];
            struct Message message;

            if (fgets(input, sizeof(input), stdin) != NULL) {
                if (true) { //just for testing
                    char *str = NULL; 
                    int outsize;
                    message.type = TEST;
                    strncpy((char *) message.source, (char *) clientId, MAX_NAME);
                    strncpy((char *) message.data, input, MAX_DATA);
                    str = serialize(message, &outsize);

                    ssize_t nbytes = send(sockfd, str, strlen(str), 0); //Haven't handled blocking send
                    if (nbytes < 0) {
                        printf("send() fail\n");
                    }
                } else {
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
    }

    // char buf[] = "message";
    // ssize_t size = sizeof(buf) / sizeof(char);
    // ssize_t nbytes = send(sockfd, buf, size, 0);
    // printf("%ld bytes sent\n", nbytes);
}

int initiomultiplex() {
    
}

int loginservice(int sockfd) {
    // char buf[] = "login"

    // send(sockfd, buf, size, 0);
}

