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

#define RESERVED_FDS 50

#define ARR_LEN (MAX_CLIENT + RESERVED_FDS)

/**
 * 
 * All sockets
 */
fd_set read_fds;  // temp file descriptor list for select()
int fdmax;        // maximum file descriptor number
int listener;     // listening socket descriptor

/**
 * User management
 */

typedef struct userStruct {
    int sessionId; // conference id
    bool deleted; // indicate if the userStruct is used
    int fd; // socket fd. Client side initiates "fin". Server doesn't
} UserStruct ; //essentially extension to fd

static struct UserManager {

    UserStruct users[MAX_CLIENT]; // contains all the users fd, check "bool deleted".
    int fdmax; // max fd
    int listenerfd; // fd of listener    
} userManager;

void user_inituserarr() {
    for (int i=0; i<MAX_CLIENT; i++) {
        userManager.users[i].deleted = true;
        userManager.users[i].fd = -1; 
        userManager.users[i].sessionId = 0; 
    }
}

int user_getOpenSlot() {
    for (int slotIdx=0; slotIdx<MAX_CLIENT; slotIdx++) {
        bool deleted = userManager.users[slotIdx].deleted;
        if (deleted) {
            return slotIdx;
        } 
    }

    return -1;
}

// rebuild fds
void user_readfdsrebuild() {
    FD_ZERO(&read_fds);
    FD_SET(listener, &read_fds);

    int tmp_fdmax = listener;

    for (int slotIdx=0; slotIdx<MAX_CLIENT; slotIdx++) {
        UserStruct userStruct = userManager.users[slotIdx];
        if (!userStruct.deleted) {
            printf("slot %d, fd %d\n", slotIdx, userStruct.fd);
            FD_SET(userStruct.fd, & read_fds);

            if (userStruct.fd > tmp_fdmax) {
                tmp_fdmax = userStruct.fd;
            }
        }
    }

    printf("fdmax: %d\n", tmp_fdmax);
    userManager.fdmax = tmp_fdmax;
}

// rewrite the current user slot
void user_addUserConn(int slotIdx, int newfd) {
    userManager.users[slotIdx].deleted = false;
    userManager.users[slotIdx].fd = newfd;
    userManager.users[slotIdx].sessionId = -1;
    // fd manipulation is done by user_readfdsrebuild() every new select
}

// even help you close(fd);
void user_deleteUserConn(int fd) {

    for (int slotIdx=0; slotIdx<MAX_CLIENT; slotIdx++) {
        UserStruct userStruct = userManager.users[slotIdx];
        if (!userStruct.deleted && userStruct.fd==fd ) {// found the correspongind connection
            printf("deleted suc\n");
            userManager.users[slotIdx].deleted = true;
            close(fd);
            return;
        }
    }
    // fd manipulation is done by user_readfdsrebuild() every new select
}

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

void broadcast(int fd, ssize_t nbytes, char *terminatedstr) {
    printf("broadcast %s\n", terminatedstr);
    for (int slotIdx=0; slotIdx<MAX_CLIENT; slotIdx++) {
        UserStruct userStruct = userManager.users[slotIdx];
        if (!userStruct.deleted && userStruct.fd != fd) {// found the correspongind connection
            if (send(userStruct.fd, terminatedstr, nbytes, 0) == -1) {
                perror("send");
            }
        }
    }
}

void clientsocketconfig(int newfd) {
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
}

/**
 * Main
 */
int main(int argc, char *argv[]) {

    user_inituserarr();
  
    int newfd;        // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;

    char buf[MAX_DATA * 2];    // buffer for client data
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN]; //idk what this is

    int yes=1;        // for setsockopt() SO_REUSEADDR, below
    int i, rv;

    struct addrinfo hints, *ai, *p;

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

    // fdmax = listener;
    userManager.fdmax = listener;
    userManager.listenerfd = listener;
    

    for(;;) {
        user_readfdsrebuild();

        if (select(userManager.fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= userManager.fdmax; i++) {
            printf("start loop with i %d\n", i);
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                
                if (i == listener) {
                    // handle new connections
                    
                    int newidx = user_getOpenSlot();
                    if (newidx == -1) {
                        printf("refused. max connection reached\n");
                        continue; // without accepting
                    }
                    
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        clientsocketconfig(newfd);

                        user_addUserConn(newidx, newfd);

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
                        printf("in %d bye!\n", i);
                        user_deleteUserConn(i);
                    } else {
                        // we got some data from a client                        
                        Message message;
                        printf("from client: %s\n", buf);
                        deserialize(buf, &message);
                        printf("from client2: %s\n", buf);

                        if (message.type == MT_LOGIN) { // Client implement: end conn if MT_LO_NAK replied
                            printf("data %s\n", message.data);
                            if (strcmp((char*)message.data, "0802") == 0) {
                                message.type = MT_LO_ACK; // Client implement: allow joinsession if MT+LO_ACK replied

                                char *strmsg = serialize(message, NULL);
                                send(i, strmsg, strlen(strmsg)*sizeof(char), 0);
                                free(strmsg);
                            } else {
                                printf("login failed\n");
                                message.type = MT_LO_NAK; // Client implement: end conn if MT_LO_NAK replied

                                char *strmsg = serialize(message, NULL);
                                send(i, strmsg, strlen(strmsg)*sizeof(char), 0);
                                free(strmsg);
                                printf("in %d bye2!\n", i);
                                user_deleteUserConn(i);
                                continue;
                            }
                        } else {
                            broadcast(i, nbytes, buf);
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!

    
    return 0;
}