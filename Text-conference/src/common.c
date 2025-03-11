#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char * 
serialize(struct Message message, int *outsize) {
    char *result = malloc(MAX_DATA * 4);
    if (!result) {
        return NULL;
    }

    //in case caller didn't assign to the size correctly
    message.size = strlen((char *) message.data);
    
    *outsize = snprintf(result, MAX_DATA * 4, "%u:%u:%s:%s", 
                        message.type, 
                        message.size, 
                        message.source, 
                        message.data
                       );
    
    return result;
}

void 
deserialize(char *instring, struct Message *outmessage) {
    char *token;
    token = strtok(instring, ":");
    outmessage->type = atoi(token);

    token = strtok(NULL, ":");
    outmessage->size = atoi(token);

    token = strtok(NULL, ":");  
    strcpy((char *) outmessage->source, token);

    char *data_start = token + strlen(token) + 1;
    memcpy(outmessage->data, data_start, outmessage->size);
    outmessage->data[outmessage->size] = '\0';  // Add null terminator after the data
}
