#ifndef COMMON_H
#define COMMON_H

/**
 * @file
 * 
 */

#define MAX_NAME 50
#define MAX_DATA 1000

struct message {
    unsigned int type; //the type of the message
    unsigned int size; //length of the data
    unsigned char source[MAX_NAME]; //ID of the client sending the message
    unsigned char data[MAX_DATA]; //
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
    QU_ACK
};

#endif // COMMON_H