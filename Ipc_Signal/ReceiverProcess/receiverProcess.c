#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h> 
#include <signal.h>

#define in_buffer_t 257

pid_t ppid;

void sigintHandler(int signum);

int main(int argc, char const *argv[])
{
    char inputBuffer[in_buffer_t];

    signal(SIGINT, sigintHandler);

    ppid = getppid();
    while(1)
    {
        read(STDIN_FILENO, inputBuffer, in_buffer_t);
        
        for (int i=0; i<3; i++)
        {
            printf("Received: %s", inputBuffer);
            sleep(1);
        }
        fflush(stdout);
        kill(ppid, SIGUSR1);
    }
    return 0;
}

void sigintHandler(int signum)
{
    exit(0);
}