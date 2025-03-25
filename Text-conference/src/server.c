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

#define MAX_SESSIONS 5

#define MAX_NAME_LEN 10


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
typedef struct Session {
    char sessionId[MAX_NAME_LEN];
    int counts; // of clients
    bool deleted;
} Session ;

typedef struct userStruct {
    char sessionId[MAX_NAME_LEN]; // conference id
    bool deleted; // indicate if the userStruct is used
    int fd; // socket fd. Client side initiates "fin". Server doesn't
} UserStruct ; //essentially extension to fd

static struct UserManager {
    UserStruct users[MAX_CLIENT]; // contains all the users fd, check "bool deleted".
    Session sessions[MAX_SESSIONS];
    int fdmax; // max fd
    int listenerfd; // fd of listener    

} userManager;

void user_inituserarr() {
    for (int i=0; i<MAX_CLIENT; i++) {
        userManager.users[i].deleted = true;
    }

    for (int slot=0; slot<MAX_SESSIONS; slot++) {
        userManager.sessions[slot].deleted = true;
    }
}

// Mode 1 : client conns, Mode 2 : sessions
int user_getOpenSlot(int mode) { 
    if (mode == 1) {
        // client conns
        printf("searching 1\n");
        for (int slotIdx=0; slotIdx<MAX_CLIENT; slotIdx++) {
            bool deleted = userManager.users[slotIdx].deleted;
            if (deleted) {
                return slotIdx;
            } 
        }

        return -1;
    } else if (mode == 2) { 
        // sessions
        printf("searching 2\n");
        for (int slotIdx=0; slotIdx<MAX_SESSIONS; slotIdx++) {
            bool deleted = userManager.sessions[slotIdx].deleted;
            if (deleted) {
                return slotIdx;
            } 
        }

        return -1;
    } else {
        printf("wrong mode\n");
        return -1;
    }
    
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
    memset(userManager.users[slotIdx].sessionId, 0, MAX_NAME_LEN); 
    // fd manipulation is done by user_readfdsrebuild() every new select
}

void user_addSession(int emptyslotIdx, const char* sessionId) {
    userManager.sessions[emptyslotIdx].deleted = false;
    userManager.sessions[emptyslotIdx].counts = 0;
    strncpy(userManager.sessions[emptyslotIdx].sessionId, sessionId, MAX_NAME_LEN - 1);
    userManager.sessions[emptyslotIdx].sessionId[MAX_NAME_LEN - 1] = '\0';
    printf("add sess %s to idx %d\n ", sessionId, emptyslotIdx);
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

// find any 0 users sessions and delete
void user_deleteSession(const char* oldid) {
    printf("try delete ses idx\n");
    printf("oldid %s\n", oldid);
    for(int slotIdx=0; slotIdx<MAX_SESSIONS; slotIdx++) {
        Session session = userManager.sessions[slotIdx];
        if (!session.deleted && strcmp(session.sessionId, oldid) == 0) {
            printf("decremented ses %s count from %d\n", oldid, userManager.sessions[slotIdx].counts);
            if (--userManager.sessions[slotIdx].counts <= 0) {
                printf("deleting ses idx\n");
                userManager.sessions[slotIdx].deleted = true;
            }
        }
    }
}

int user_getsessionIndex(const char* sessionId) {
    printf("ses idx req\n");
    for(int slotIdx=0; slotIdx<MAX_SESSIONS; slotIdx++) {
        Session session = userManager.sessions[slotIdx];
        if (!session.deleted && strcmp(session.sessionId, sessionId) == 0) {
            printf("found ses idx\n");
            return slotIdx;
        }
    }
    printf("ses %s not found\n", sessionId);
    return -1;
}

int user_getuserIndex(int fd) {
    for (int slotIdx=0; slotIdx<MAX_CLIENT; slotIdx++) {
        UserStruct userStruct = userManager.users[slotIdx];
        if (!userStruct.deleted && userStruct.fd==fd) {// found the correspongind connection
            return slotIdx;
        }
    }

    return -1;
} 

void user_joinsession(int fd, const char* sessionId) {
    for (int slotIdx = 0; slotIdx < MAX_CLIENT; slotIdx++) {
        UserStruct user = userManager.users[slotIdx];
        if (!user.deleted && user.fd==fd) {
            strncpy(userManager.users[slotIdx].sessionId, sessionId, MAX_NAME_LEN - 1);
            userManager.users[slotIdx].sessionId[MAX_NAME_LEN - 1] = '\0';
            int idx = user_getsessionIndex(sessionId);
            if (idx >= 0) {
                int debug = ++userManager.sessions[idx].counts;
                printf("increement ses %s count to %d\n", sessionId, debug);
            }
        }
    }
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
    char sessionId[MAX_NAME_LEN];
    int idx = user_getuserIndex(fd);
    if (idx<0) {
        printf("non session user tried to broadcast\n");
        return;
    }

    strncpy(sessionId, userManager.users[idx].sessionId, MAX_NAME_LEN-1);
    sessionId[MAX_NAME_LEN-1] = '\0';

    printf("broadcast %s\n", terminatedstr);
    for (int slotIdx=0; slotIdx<MAX_CLIENT; slotIdx++) {
        UserStruct userStruct = userManager.users[slotIdx];
        if (!userStruct.deleted && userStruct.fd != fd && 
            strcmp(userStruct.sessionId, sessionId) == 0) {
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

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <argument>\n", argv[0]);
        exit(1);
    }

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
    if ((rv = getaddrinfo(NULL, argv[1], &hints, &ai)) != 0) {
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
                    
                    int newidx = user_getOpenSlot(1);
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
                    // nbytes = recv(i, buf, sizeof buf, 0);
                    nbytes = recvMessage(i, buf, sizeof buf);
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
                        int idx = user_getuserIndex(i);
                        if (idx >= 0) {
                            char oldsessionid[MAX_NAME];
                            strncpy(oldsessionid, userManager.users[idx].sessionId, MAX_NAME-1);
                            oldsessionid[MAX_NAME-1] = '\0';
                            user_deleteSession(oldsessionid);
                            user_deleteUserConn(i);
                        }
                    } else {
                        // we got some data from a client                        
                        Message message;
                        printf("from client: %s\n", buf);
                        deserialize(buf, &message);
                        printf("deserialized: %s\n", buf);

                        if (message.type == MT_LOGIN) { // Client implement: end conn if MT_LO_NAK replied
                            printf("data %s\n", message.data);

                            // check password 
                            if (strcmp((char*)message.data, "0802") == 0) {
                                message.type = MT_LO_ACK; // Client implement: allow joinsession if MT+LO_ACK replied

                                int fd = i;
                                sendMessage(fd, message);
                            } else {
                                    printf("login failed\n");
                                message.type = MT_LO_NAK; // Client implement: end conn if MT_LO_NAK replied
                                int fd = i;
                                sendMessage(fd, message);
                                    printf("in %d bye2!\n", fd);
                                user_deleteUserConn(fd);
                                continue;
                            }
                        } else if (message.type == MT_NEW_SESS) {
                            printf("user new session in\n");
                            int emptyIdx = user_getOpenSlot(2);
                            if (emptyIdx >= 0) {
                                // Not handling duplicate session
                                user_addSession(emptyIdx, (char *)message.data);
                                message.type = MT_NS_ACK;
                                sendMessage(i, message);
                            } else {
                                // no need to reply. client doesn't need to know.
                            }
                        } else if (message.type == MT_JOIN) {
                            printf("user joins\n");
                            int idx = user_getsessionIndex((char *)message.data);
                            if (idx >= 0) {
                                user_joinsession(i, (char *)message.data);
                                message.type = MT_JN_ACK;
                                sendMessage(i, message); // msg.data alr set by client
                            } else {
                                message.type = MT_JN_NAK;
                                sendMessage(i, message); // msg.data alr set by client
                            }
                        } else if (message.type == MT_LEAVE_SESS) {
                            printf("user leave session\n");
                            int idx = user_getuserIndex(i);
                            if (idx >= 0) {
                                char oldid[MAX_NAME_LEN];
                                strncpy(oldid, userManager.users[idx].sessionId, MAX_NAME_LEN - 1);
                                oldid[MAX_NAME_LEN - 1] = '\0';
                                memset(userManager.users[idx].sessionId, 0, MAX_NAME_LEN);
                                user_deleteSession(oldid);
                            }
                        }
                        else if (message.type == MT_QUERY) {
                            printf("qury req\n");
                            for (int slotIdx=0; slotIdx<MAX_CLIENT; slotIdx++) {
                                UserStruct userStruct = userManager.users[slotIdx];
                                if (!userStruct.deleted) {// found the correspongind connection
                                    printf("user %d in session %s\n", userStruct.fd, userStruct.sessionId);
                                }
                            }

                            printf("Session: ");
                            for (int slotIdx=0; slotIdx<MAX_SESSIONS; slotIdx++) {
                                if (!userManager.sessions[slotIdx].deleted) {
                                    printf("%s ", userManager.sessions[slotIdx].sessionId);
                                }
                            }
                            printf("\n");

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

