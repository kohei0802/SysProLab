#include <stdio.h>
#include <unistd.h>
#include <sys/types.h> 
#include <signal.h>

#define in_buffer_t 257

pid_t ppid;

int main(int argc, char const *argv[])
{
    char inputBuffer[in_buffer_t];

    ppid = getppid();
    while(1)
    {
        read(STDIN_FILENO, inputBuffer, in_buffer_t);
        printf("Received: %s", inputBuffer);
        fflush(stdout);
        kill(ppid, SIGUSR1);
    }
    return 0;
}
