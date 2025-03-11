#ifndef COMMON_H
#define COMMON_H

#define MAX_PASSWORDLEN 256
#define MAX_NAME 50
#define MAX_DATA 100

struct Credential {
    int clientid;
    char password[MAX_PASSWORDLEN];
};

struct Message {
    unsigned int type; //type of the message,
    unsigned int size; //length of the data.
    unsigned char source[MAX_NAME]; //ID of the client sending the message.
    unsigned char data[MAX_DATA]; 
};

enum MessageType {
    LOGIN,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK,
    TEST
};

char * 
serialize(struct Message message, int *outsize);

void 
deserialize(char *instring, struct Message *outmessage) ;

#endif // COMMON_H