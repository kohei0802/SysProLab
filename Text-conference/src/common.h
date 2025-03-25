#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

#define MAX_PASSWORDLEN 256
#define MAX_NAME 50
#define MAX_DATA 2000

struct Credential {
    int clientid;
    char password[MAX_PASSWORDLEN];
};

typedef struct Message {
    unsigned int type; //type of the message,
    unsigned int size; //length of the data.
    unsigned char source[MAX_NAME]; //ID of the client sending the message.
    unsigned char data[MAX_DATA]; 
} Message;

enum MessageType {
    MT_LOGIN,
    MT_LO_ACK,
    MT_LO_NAK,
    MT_EXIT,
    MT_JOIN,
    MT_JN_ACK,
    MT_JN_NAK,
    MT_LEAVE_SESS,
    MT_NEW_SESS,
    MT_NS_ACK,
    MT_MESSAGE,
    MT_QUERY,
    MT_QU_ACK,
    MT_TEST, 
    MT_REGISTER,
    MT_MESSAGE_PRIVATE,
};

char * 
serialize(struct Message message, int *outsize);

/**
 * User must null-terminate the data[] of Message
 */
void 
deserialize(char *instring, struct Message *outmessage) ;

void 
sendMessage(int sockfd, Message outgoingmsg);

ssize_t 
recvMessage(int sockfd, char *buf, size_t bufsize);

#endif // COMMON_H