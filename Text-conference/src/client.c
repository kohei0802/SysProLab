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

int sockfd;

struct sockaddr_in servaddr;

int initsocket();

int main(int argc, char *argv[]) {

    initsocket();

    while (true) {
        /* code */
        
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

    char buf[] = "message";
    ssize_t size = sizeof(buf) / sizeof(char);
    ssize_t nbytes = send(sockfd, buf, size, 0);
    printf("%ld bytes sent\n", nbytes);

    while (true)
    {
        char buf[] = "message";
        ssize_t size = sizeof(buf) / sizeof(char);
        ssize_t nbytes = send(sockfd, buf, size, 0);
        printf("%ld bytes sent\n", nbytes);

        sleep(1);
    }
    

}

int loginservice(int sockfd) {
    // char buf[] = "login"

    // send(sockfd, buf, size, 0);
}