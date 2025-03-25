#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdbool.h>


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

/**
 * Receives a complete message by reading until the full data length is received.
 * Returns total bytes received or -1 on error.
 */
ssize_t
recvMessage(int sockfd, char *buf, size_t bufsize) {
    ssize_t total_bytes = 0;
    ssize_t bytes_received;
    char *working_copy;
    char *token;
    unsigned int expected_size;
    char temp_buf[1024]; 
    
    // Receive initial chunk to get the message size
    bytes_received = recv(sockfd, buf, bufsize - 1, 0);
    if (bytes_received <= 0) {
        return bytes_received;
    }
    
    buf[bytes_received] = '\0';
    total_bytes = bytes_received;
    
    // Parse the size field from the message
    working_copy = strdup(buf);
    if (!working_copy) {
        return -1;
    }
    
    // Skip the type field
    token = strtok(working_copy, ":");
    if (!token) {
        free(working_copy);
        return -1;
    }
    
    // Get the size field
    token = strtok(NULL, ":");
    if (!token) {
        free(working_copy);
        return -1;
    }
    
    expected_size = atoi(token);
    free(working_copy);
    
    // Find start of actual data (after last colon)
    char *last_colon = strrchr(buf, ':');
    if (!last_colon) {
        return -1;
    }
    last_colon++; // Move past the colon
    
    // Calculate how many bytes of actual data we've received
    size_t received_data_size = strlen(last_colon);
    size_t remaining_data = expected_size - received_data_size;
    bool buffer_full = false;
    
    // Keep receiving until we get exactly the remaining data bytes
    while (remaining_data > 0) {
        size_t to_receive = remaining_data;
        
        if (!buffer_full) {
            // Still have room in main buffer
            size_t remaining_space = bufsize - total_bytes - 1;
            if (remaining_space == 0) {
                buffer_full = true;
                continue;
            }
            
            // Only receive what we need
            to_receive = (remaining_space < remaining_data) ? remaining_space : remaining_data;
            bytes_received = recv(sockfd, buf + total_bytes, to_receive, 0);
        } else {
            // Buffer is full, receive into temp buffer
            to_receive = (sizeof(temp_buf) < remaining_data) ? sizeof(temp_buf) : remaining_data;
            bytes_received = recv(sockfd, temp_buf, to_receive, 0);
        }
        
        if (bytes_received <= 0) {
            return bytes_received;
        }
        
        if (!buffer_full) {
            total_bytes += bytes_received;
            buf[total_bytes] = '\0';
        }
        
        remaining_data -= bytes_received;
    }
    
    return total_bytes;
}