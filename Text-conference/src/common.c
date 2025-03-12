#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>


/**
 * User has to free() on his own
 */
char * 
serialize(struct Message message, int *outsize) {
    char *result = malloc(MAX_DATA * 4);
    int cpbytes;
    if (!result) {
        return NULL;
    }

    //in case caller didn't assign to the size correctly
    message.size = strlen((char *) message.data);
    
    cpbytes = snprintf(result, MAX_DATA * 4, "%u:%u:%s:%s", 
                        message.type, 
                        message.size, 
                        message.source, 
                        message.data
                       );
    if (outsize) {
        *outsize = cpbytes;     
    }
    return result;
}

void 
deserialize(char *instring, struct Message *outmessage) {
    // Make a copy of instring so we don't modify the original
    char *working_copy = strdup(instring);
    if (working_copy == NULL) {
        perror("strdup failed");
        return;
    }
    
    char *token;
    token = strtok(working_copy, ":");
    outmessage->type = atoi(token);

    token = strtok(NULL, ":");
    outmessage->size = atoi(token);
    
    token = strtok(NULL, ":");  
    strcpy((char *) outmessage->source, token);

    char *data_start = token + strlen(token) + 1;
    memcpy(outmessage->data, data_start, outmessage->size);
    outmessage->data[outmessage->size] = '\0';

    // Free the copy
    free(working_copy);
}


void 
sendMessage(int sockfd, Message outgoingmsg) {
    char *strmsg = serialize(outgoingmsg, NULL);
    send(sockfd, strmsg, strlen(strmsg)*sizeof(char), 0);
    free(strmsg);
}
