#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>


#define msg_t 100

pid_t childPid;

void signalHandler(int signum);

void clearStdin();


int main(int argc, char const *argv[])
{
    pid_t pid1;
    int pipe1[2];
    char inputBuffer[msg_t];

    pipe(pipe1);
    pid1 = fork();

    if (pid1 == 0)
    {
        dup2(pipe1[0], STDIN_FILENO);
        close(pipe1[0]);
        close(pipe1[1]);

        execl("./receiver", "receiver", NULL);
    }
    else if (pid1 > 0)
    {
        signal(SIGINT, signalHandler);

        childPid = pid1;
        printf("Hello World from sender\n");
        while(1) 
        {
            if (!fgets(inputBuffer, sizeof(inputBuffer), stdin))
                break;

            if (stdin->_IO_read_ptr == stdin->_IO_read_end)
            {
                //Do nothing. No need to clear stdin buffer
            }
            else 
            {
                clearStdin();
            }
            

            write(pipe1[1], inputBuffer, msg_t);
        }
        wait(NULL);
    }
    else 
    {
        printf("BAD!");
        return 1;
    }


    return 0;
}

void signalHandler(int signum)
{
    kill(childPid, SIGINT);
    exit(0);
}

void clearStdin() {
    /* 
        Don't call this if the stdin input buffer is already empty
        because it will cause I/O block when calling getchar()
    */
    int c;
    while ((c=getchar()) != '\n') {}
}
