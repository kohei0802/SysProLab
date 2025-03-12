#ifndef IPC_H
#define IPC_H

#define BUFFERSIZE 100

struct packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char* filename; //NULL TERMINATION NEEDED!
    char filedata[1000];
};

#endif