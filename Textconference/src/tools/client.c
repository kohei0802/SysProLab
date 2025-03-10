#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define DEBUG

/**
 * @file
 * 
 */

typedef struct addrinfo Addr_metadata;
typedef struct servaddr Address;

int sockfd;
addrinfo sockconfig;
Addr_metadata *server=NULL;

int initsock(int argc, char *argv[]);

int main(int argc, char *argv[]) {

   initsock(argc, argv);

    while (true) {
        /* code */

    }
    

    return 0;
}

int initsock(int argc, char *argv[]) {
    char serv_addr[] = "127.0.0.1";
    char serv_port[] = "2345"; 

    //Socket setup
    memse(&sockconfig, 0, sizeof sockconfig);
    sockconfig.ai_family = AF_INET;
    sockconfig.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(serv_addr, serv_port, &sockconfig, &server) != 0) 
    {
        perror("Address resolution error");
        return 1;
    }

}