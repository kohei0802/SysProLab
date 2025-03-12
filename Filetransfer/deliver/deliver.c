#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include "deliver.h"
#include "../ipc.h"

// #define DEBUG

typedef struct addrinfo Addr_metadata;
typedef struct sockaddr_storage Address;
typedef struct usec_timeouter {
    float estimated_timeout;
    float dev_RTT;
    float timeout_interval;
} Usec_timeouter;


void send_file(FILE *file, char *filename);
// return -1 if error, 0 if success
int extract_ftpfilename(char *cmd_input, FILE** file_out);
int setup_main(int argc, char *argv[]);
long get_elapsed_usec(struct timespec start, struct timespec end);
/* if, sample_RTT < 0, consider last request timeouted, 
in which case we double the timeout */
void set_timeestimate(Usec_timeouter *timeouter, float sample_RTT);
void set_timeout(int sockfd, Usec_timeouter timeoutstruct);
ssize_t wrecvfrom (int fragno, int __fd, void *__restrict __buf, size_t __n, int __flags,
	  __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len);

//Socket setup
int sockfd;
Addr_metadata address_criteria, *resolved_address=NULL, *p=NULL;

Usec_timeouter timeouter = {0, 8, 0};

//File I/O
char user_command[FILE_NAME_SIZE] = "";
char file_name[FILE_NAME_SIZE] = "";
FILE* file_hand = NULL;
char fileio_buf[1000];

int main(int argc, char *argv[]) {
    if (setup_main(argc, argv) != 0){
        return 1;
    }

    send_file(file_hand, file_name);
    
    //UDP socket close
    freeaddrinfo(resolved_address);
    close(sockfd);

    if (file_hand)
        fclose(file_hand);

    return 0;
}

int setup_main(int argc, char *argv[])
{
    #ifdef DEBUG
    char serv_addr[] = "127.0.0.1";
    char serv_port[] = "2345"; 
    #else 
    if (argc != 3)
    {
        printf("deliver <server address> <server port number>\n");
        return 1;
    }
    #endif
    //Get the file
    if (fgets(user_command, FILE_NAME_SIZE, stdin) == NULL) {
        fprintf(stderr, "Error reading input\n");
        return 1;
    }
    //because the user input will include a "\n"
    user_command[strcspn(user_command, "\n")] = '\0'; 

    //modify global file information  
    if (extract_ftpfilename(user_command, &file_hand) != 0) {
        perror("command invalid\n");
        return 1;
    }

    //Socket setup
    memset(&address_criteria, 0, sizeof address_criteria);
    address_criteria.ai_family = AF_INET;
    address_criteria.ai_socktype = SOCK_DGRAM;
    #ifdef DEBUG
    if (getaddrinfo(serv_addr, serv_port, &address_criteria, &resolved_address) != 0) 
    {
        perror("Address resolution error");
        return 1;
    }
    #else 
    if (getaddrinfo(argv[1], argv[2], &address_criteria, &resolved_address) != 0) 
    {
        perror("Address resolution error");
        return 1;
    }
    #endif
    
    if ((sockfd = socket(resolved_address->ai_family, resolved_address->ai_socktype, resolved_address->ai_protocol)) == -1) 
    {
        perror("socket");
        return 1;
    }

    if (sendto(sockfd, "ftp", strlen("ftp"), 0, resolved_address->ai_addr, resolved_address->ai_addrlen) == -1) 
    {
        perror("sendto");
        return 1;
    }

    struct timespec start_time, end_time;
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
        // Check server reply
    char buffer[BUFFERSIZE] = {0};
    ssize_t numbytes = recvfrom(sockfd, buffer, BUFFERSIZE-1, 0, NULL, NULL);
    buffer[numbytes] = '\0';
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long sample_RTT = get_elapsed_usec(start_time, end_time);
    
    if (strcmp(buffer, "yes") == 0) {
        printf("File transfer can start with rtt %ld µs\n", sample_RTT);
    } else {
        printf("Wrong type of server\n");
        return 1;
    }

    // init timeouter
    timeouter.dev_RTT = 8;
    timeouter.estimated_timeout = sample_RTT;
    timeouter.timeout_interval = timeouter.estimated_timeout + 5 * timeouter.dev_RTT;

    return 0;
}


// Creates packet string in format "total_frag:frag_no:size:filename:filedata"
char* create_packet_string(struct packet *pkt, unsigned int file_data_size, size_t* pkt_size) {
    char *result = malloc(sizeof(char) * (1024 + strlen(pkt->filename)));
    sprintf(result, "%d:%d:%d:%s:", pkt->total_frag, pkt->frag_no, pkt->size, pkt->filename);
    int header_len = strlen(result);
    memcpy(result + header_len, pkt->filedata, file_data_size);

    //output pkt_size
    *pkt_size = header_len + file_data_size;

    //output pkt_string
    return result;
}

void send_file(FILE *file, char *filename) {
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    //Just need to use one pkt for all
    struct packet pkt;
    pkt.filename = filename;
    pkt.total_frag = (file_size + 999) / 1000; //round-up division

    for(int frag_no=1; frag_no <= pkt.total_frag; frag_no++){
        //Read from file
        size_t bytes_read = fread(pkt.filedata, 1, 1000, file);
        pkt.size = bytes_read; //This information is useful for create_packet_string
        pkt.frag_no = frag_no;
        // Create and send packet
        size_t pkt_size;
        char *packet_str = create_packet_string(&pkt, bytes_read, &pkt_size);

        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        int retries = 0;
        while(true) {
            sendto(sockfd, packet_str, pkt_size, 0, 
                resolved_address->ai_addr, resolved_address->ai_addrlen);
            // #ifdef DEBUG
            printf("Sent %d\n", pkt.frag_no);
            // #endif

            char ack[10];
            ssize_t bytes_received = wrecvfrom(frag_no, sockfd, ack, sizeof(ack), 0, NULL, NULL);

            if (bytes_received > 0) {
                break;
            }
            // if bytes-received <= 0, it timeouted. 
            printf("Timeout occurred. Retransmitting packet %d\n", frag_no);
            set_timeestimate(&timeouter, -1);
            retries++;
        }
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        if (retries == 0) 
            set_timeestimate(&timeouter, get_elapsed_usec(start_time, end_time));

        free(packet_str);
    }
}

int extract_ftpfilename(char *cmd_input, FILE** file_out)
{
    // Check if input starts with "ftp "
    if (strncmp(cmd_input, "ftp ", 4) != 0) {
        fprintf(stderr, "Invalid format\n");
        return -1;
    }

    // Check if pointer is valid
    if (file_out == NULL) {
        return -1;
    }

    //copy the string but skip the first 4 characters
    strncpy(file_name, cmd_input + 4, FILE_NAME_SIZE - 1); 
    *file_out = fopen(file_name, "r");

    if (!*file_out) {
        perror("FILE doesn't exist");
        return -1;
    }

    return 0;
}

long get_elapsed_usec(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000000 + 
           (end.tv_nsec - start.tv_nsec) / 1000;
}

void set_timeestimate(Usec_timeouter *timeouter, float sample_RTT) {
    if (sample_RTT < 0) {
        timeouter->timeout_interval *= 2;
        return;
    } 

    timeouter->estimated_timeout = 0.875 * timeouter->estimated_timeout + 0.125 * sample_RTT;
    timeouter->dev_RTT = 0.75 * timeouter->dev_RTT + 0.25 * abs(sample_RTT - timeouter->estimated_timeout);
    timeouter->timeout_interval = timeouter->estimated_timeout + 4 * timeouter->dev_RTT;
    
}

/* 
    This function makes sure when it returns, it'll be about the latest requested seq OR because of 
    its timeout
    not because of the old Ack or error
*/
ssize_t wrecvfrom (int fragno, int __fd, void *__restrict __buf, size_t __n, int __flags,
    __SOCKADDR_ARG __addr, socklen_t *__restrict __addr_len)
{
    // char expected_ack[10];
    // sprintf(expected_ack, "%d", fragno);  

    char ack[10];
    struct timespec start, end;
    float timeout = timeouter.timeout_interval; 
    ssize_t bytes_received;

    set_timeout(sockfd, timeouter); 

    int flag = 0;
    do {
        if (timeouter.timeout_interval <= 0) {
            flag = 1;
            break;
        }

        clock_gettime(CLOCK_MONOTONIC, &start);
        printf("Just sent one %d with %f left\n", fragno, timeout);
        bytes_received = recvfrom(sockfd, ack, sizeof(ack), 0, NULL, NULL);
        printf("Received one %d with %f left\n", fragno, timeout);
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        timeout -= get_elapsed_usec(start, end);

        if (bytes_received == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                //timeout
                flag = 1;
            }else if (errno == EINTR) {
                set_timeout(sockfd, (Usec_timeouter) {.timeout_interval = timeout});
            }else {
                perror("socket recvfrom error");
                exit(1);
            }
        } else {
            ack[bytes_received] = '\0';
            if (atoi(ack) == fragno) {
                flag = 2;
            }else {
                set_timeout(sockfd, (Usec_timeouter) {.timeout_interval = timeout}); 
            }
        }
    } while(!flag);  //if flag != 0, stops

    return bytes_received;
}

void set_timeout(int sockfd, Usec_timeouter timeoutstruct) {
    struct timeval tv;
    tv.tv_sec = (long)(timeoutstruct.timeout_interval / 1000 / 1000);  
    tv.tv_usec = (long)(timeoutstruct.timeout_interval) % (1000 * 1000);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));
}