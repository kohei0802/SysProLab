#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>


#define msg_t 257

pid_t childPid;

void sigintHandler(int signum);
void sigusr1Handler(int signum);

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
        sigset_t set;
        siginfo_t sigInformation;

        sigemptyset(&set);
        sigaddset(&set, SIGUSR1);

        childPid = pid1;

        //Handle signal to kill child
        signal(SIGINT, sigintHandler);
        signal(SIGUSR1, sigusr1Handler);

        printf("%d\n", getpid());
        while(1) 
        {
            printf(">>> ");
            if (!fgets(inputBuffer, sizeof(inputBuffer), stdin))
                break;
            // if (!(stdin->_IO_read_ptr == stdin->_IO_read_end))
            //     clearStdin();

            write(pipe1[1], inputBuffer, msg_t);

            do
            {
                sigwaitinfo(&set, &sigInformation);
            } while (sigInformation.si_pid != childPid);

            if (!(stdin->_IO_read_ptr == stdin->_IO_read_end))
                clearStdin();
            
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

void sigintHandler(int signum)
{
    kill(childPid, SIGINT);
    exit(0);
}

void sigusr1Handler(int signum) {;}

void clearStdin() {
    /* 
        Don't call this  if the stdin input buffer is already empty
        because it will cause I/O block when calling getchar()
    */
    int c;
    while ((c=getchar()) != '\n') {}
}
