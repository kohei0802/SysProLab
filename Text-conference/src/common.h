#ifndef COMMON_H
#define COMMON_H

#define MAX_PASSWORDLEN 256
#define MAX_NAME 50
#define MAX_DATA 100

struct credential {
    int clientid;
    char password[MAX_PASSWORDLEN];
};

struct message {
    unsigned int type; //type of the message,
    unsigned int size; //length of the data.
    unsigned char source[MAX_NAME]; //ID of the client sending the message.
    unsigned char data[MAX_DATA]; 
};

#endif // COMMON_H