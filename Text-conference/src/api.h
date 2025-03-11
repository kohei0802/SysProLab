#ifndef API_H
#define API_H

#include "common.h"
// Your code here

int sendText(int sockfd, Message message);

int newSession(int sockfd, Message message);



#endif // API_H