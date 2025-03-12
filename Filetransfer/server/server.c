#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "server.h"
#include "../ipc.h"

// #define DEBUG

typedef struct addrinfo Addr_metadata;
typedef struct sockaddr_storage Address;

typedef struct ftp_client {
    Address client_addr;
    socklen_t addr_len;
} FTP_client;

int setup_main(int argc, char *argv[]);
void handle_file_transfer(void);

int sockfd;
Addr_metadata address_criteria, *resolved_address; //modern, protocol-independent way to handle both IPv4 and IPv6
char *listen_port;


int main(int argc, char *argv[]) {
    srand(time(NULL));  

    if (setup_main(argc, argv) != 0){
        return 1;
    }

    char recv_buf[BUFFERSIZE] = {0}; 
    FTP_client ftp_client;
    memset(&ftp_client, 0, sizeof ftp_client);
    ftp_client.addr_len = sizeof(Address);  //has to initialize to be a large address


 
    ssize_t bytes_received = recvfrom(sockfd, recv_buf, BUFFERSIZE-1, 0, (struct sockaddr *)&ftp_client.client_addr, &(ftp_client.addr_len)); 
    recv_buf[bytes_received] = '\0';

    if (strcmp(recv_buf, "ftp")==0)
    {
        sendto(sockfd, "yes", 3, 0, (struct sockaddr *)&ftp_client.client_addr, ftp_client.addr_len);
    }
    else
    {
        sendto(sockfd, "no", 2, 0, (struct sockaddr *)&ftp_client.client_addr, ftp_client.addr_len);
    }

    printf("Received: %s\n", recv_buf);

    handle_file_transfer();


    close(sockfd);
    freeaddrinfo(resolved_address);
    return 0;
}

int setup_main(int argc, char *argv[])
{
    #ifdef DEBUG
    listen_port = "2345";
    #else
    if (argc != 2)
    {
        printf("Usage: server <UDP listen port>\n");
        exit(1);
    }
    listen_port = argv[1];
    #endif

    memset(&address_criteria, 0, sizeof address_criteria);
    address_criteria.ai_family = AF_INET6;    // IPv6 or IPv4
    address_criteria.ai_socktype = SOCK_DGRAM; // UDP
    address_criteria.ai_flags = AI_PASSIVE;    // Use my IP
    
    getaddrinfo(NULL, listen_port, &address_criteria, &resolved_address);
    sockfd = socket(resolved_address->ai_family, 
                    resolved_address->ai_socktype, 
                    resolved_address->ai_protocol);
    bind(sockfd, resolved_address->ai_addr, resolved_address->ai_addrlen);

    return 0;
}

void parse_packet(char *packet_str, size_t packet_size, struct packet *pkt) {
    char *token;
    char *header_end;
    int colon_count = 0;

    for (size_t i=0; i<packet_size; i++){
        if (packet_str[i] == ':') {
            colon_count++;
            if (colon_count == 4) {
                header_end = &packet_str[i];
                break;
            }
        }
    }

    char tmp_saved_char = *header_end;
    *header_end = '\0';

    // Parse total_frag
    token = strtok(packet_str, ":");
    pkt->total_frag = atoi(token);
    
    // Parse frag_no
    token = strtok(NULL, ":");
    pkt->frag_no = atoi(token);
    
    // Parse size
    token = strtok(NULL, ":");
    pkt->size = atoi(token);
    
    // Parse filename
    token = strtok(NULL, ":");
    pkt->filename = strdup(token);

    // Restore the original character
    *header_end = tmp_saved_char;
    
    // Copy binary data (starts after the fourth colon)
    memcpy(pkt->filedata, header_end + 1, pkt->size);
}

void handle_file_transfer() {
    char buffer[2048];
    struct packet pkt;
    FILE *output_file = NULL;
    
    unsigned int expected_fragno = 1;
    while(1) {
        struct sockaddr_storage client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        ssize_t bytes_received = recvfrom(sockfd, buffer, sizeof(buffer)-1, 0,
                                        (struct sockaddr *)&client_addr, &addr_len);

        if (rand() % 100 + 1 <= 5) {
            printf("Packet dropped\n");
            continue;  // Drop packet
        }

        if(bytes_received > 0) { 
            parse_packet(buffer, bytes_received, &pkt);

            // #ifdef DEBUG
            printf("rec packed %d\n", pkt.frag_no);
            // #endif
            
            if (pkt.frag_no != expected_fragno)
                continue;
            else 
                expected_fragno++;

            // Open file if this is the first fragment
            if(pkt.frag_no == 1) {
                output_file = fopen(pkt.filename, "wb");
            }
            
            // Write data to file
            if(output_file) {
                fwrite(pkt.filedata, 1, pkt.size, output_file);
            }
            
            // Send ACK
            char ack[10]; 
            sprintf(ack, "%d", pkt.frag_no);
            sendto(sockfd, ack, strlen(ack), 0, 
                   (struct sockaddr *)&client_addr, addr_len);
            
            // Close file if this was the last fragment
            if(pkt.frag_no == pkt.total_frag) {
                if(output_file) {
                    fclose(output_file);
                    printf("File transfer complete: %s\n", pkt.filename);
                }
                free(pkt.filename);
                return;  // Exit function when done
            }
        }
    }
}


