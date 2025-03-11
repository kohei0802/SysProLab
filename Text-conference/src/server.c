#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> // Needed for memset
#include <netinet/ip.h> /* superset of previous */
#include <unistd.h>
#include <arpa/inet.h>
 #include <sys/types.h>
 #include <netdb.h>
 #include <netinet/tcp.h>
#include "common.h"

/**
 * @file
 * 
 */

#define PORT "2345"  

#define MAX_CLIENT 50

/**
 * User management
 */
static int usercount = 0; //to limit total clients

typedef struct userStruct {
    int sessionId;
} UserStruct; //essentially extension to fd

static UserStruct users[MAX_CLIENT + 50]; //use fd as the index, 50 is arbitrary

/**
 * 
 * All sockets
 */
fd_set master;    // master file descriptor list
fd_set read_fds;  // temp file descriptor list for select()
int fdmax;        // maximum file descriptor number
int listener;     // listening socket descriptor

/**
 * Address formatting
 */
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void broadcast(int i, ssize_t nbytes, char *terminatedstr) {
    
        // except the listener and ourselves
        for(int j = 0; j <= fdmax; j++) {
                            // send to everyone!
            if (FD_ISSET(j, &master)) {
                if (j != listener && j != i) {
                    if (send(j, terminatedstr, nbytes, 0) == -1) {
                        perror("send");
                    }
                }
            }
            
        }
        
    
}

/**
 * Main
 */
int main(int argc, char *argv[]) {
    // sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // memset(&servaddr, 0, sizeof(servaddr));
    // servaddr.sin_family = AF_INET;
    // servaddr.sin_port = htons(2345);
    // bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr));

    // listen(sockfd, 10);

  
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[MAX_DATA * 2];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN]; //idk what this is

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }
    
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) { 
            continue;
        }
        
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }
 
    // add the listener to the master set
    FD_SET(listener, &master);

    fdmax = listener;

    for(;;) {
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    
                    if (usercount >= MAX_CLIENT) {
                        continue;
                    }
                    
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        int keepalive = 1;
                        int keepcnt = 3;        // Number of probes before connection is declared dead
                        int keepidle = 30;      // Seconds before sending keepalive probes
                        int keepintvl = 5;      // Seconds between keepalive probes

                        if (setsockopt(newfd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
                            perror("setsockopt(SO_KEEPALIVE)");
                        }

                        // Set the number of probes before connection is declared dead
                        if (setsockopt(newfd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
                            perror("setsockopt(TCP_KEEPCNT)");
                        }

                        // Set the time before sending keepalive probes
                        if (setsockopt(newfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
                            perror("setsockopt(TCP_KEEPIDLE)");
                        }

                        // Set the interval between keepalive probes
                        if (setsockopt(newfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
                            perror("setsockopt(TCP_KEEPINTVL)");
                        }

                        usercount += 1;

                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }

                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {
                    // handle data from a client
                    nbytes = recv(i, buf, sizeof buf, 0);
                    buf[nbytes]='\0';
                    if (nbytes <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("selectserver: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                        usercount -= 1;
                    } else {
                        // we got some data from a client
                        printf("%s", buf);
                        broadcast(i, nbytes, buf);
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!

    
    return 0;
}