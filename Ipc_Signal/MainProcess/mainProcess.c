#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#include <stdbool.h>


#define MSG_SIZE 257

#define MON_INT 300

pid_t childPid=0;
bool inferencing=false; //can be changed to semaphore for efficiency

void sigintHandler(int signum);
void sigusr1Handler(int signum);

void clearStdin();

void *threadMonitor(void *arg)
{
    const int INPUTSIZE = 50;
    char output[INPUTSIZE*10];
    FILE* procFile = NULL;
    char procPath[INPUTSIZE];
    char procText[INPUTSIZE];

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    snprintf(procPath, sizeof(procPath), "/proc/%d/stat", childPid);
    
    /*(1)pid (2)comm (3)state (14)utime (15)stime (19)nice (23)vsize (39)processor (41)policy
    */
    const char format[]="%s %s %s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %s %s %*s %*s %*s %s %*s %*s %*s %s \
                            %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %s %*s %s";
    char pid[INPUTSIZE], comm[INPUTSIZE], state[INPUTSIZE], utime[INPUTSIZE], stime[INPUTSIZE]
                , nice[INPUTSIZE], vsize[INPUTSIZE], processor[INPUTSIZE], policy[INPUTSIZE];

    procFile=fopen(procPath, "r");
    fscanf(procFile, format, pid, comm, state, utime, stime, nice, vsize, processor, policy);
    fclose(procFile);

    float initialTimeUsage = (float) atoi(utime) + (float) atoi(stime);
    float initialTime = 
    usleep(1000 * MON_INT);

    while(true) 
    {
        procFile=fopen(procPath, "r");
        fscanf(procFile, format, pid, comm, state, utime, stime, nice, vsize, processor, policy);
        
        //Calculate cpu usage in %
        float currentUsage = (float)(atoi(utime)+atoi(stime));
        float cpuusage = (currentUsage-initialTimeUsage) / 30 * 100;
        initialTimeUsage = currentUsage;
    
        snprintf(output, sizeof(output), "%s %s %s %s %s %s %s %s %s %.2f%%", 
                    pid, comm, state, policy, nice, vsize, processor, utime, stime, (cpuusage>100.0)?100.0:cpuusage);
        fwrite(output, sizeof(char), strlen(output), stderr);
        fwrite("\n", sizeof(char), 1, stderr);
        fflush(stderr);
        fclose(procFile);

        usleep(1000 * MON_INT);
        
        pthread_testcancel();
    }
    
}

int main(int argc, char const *argv[])
{
    pid_t pid1;
    int requestPipe[2];
    char inputBuffer[MSG_SIZE];

    pipe(requestPipe);
    pid1 = fork();

    if (pid1 == 0)
    {
        dup2(requestPipe[0], STDIN_FILENO);
        close(requestPipe[0]);
        close(requestPipe[1]);

        execl("./receiver", "receiver", NULL);
    }
    else if (pid1 > 0)
    {
        sigset_t set;
        siginfo_t sigInformation;
        pthread_t monitorThread;

        childPid = pid1;

        signal(SIGINT, sigintHandler);
        signal(SIGUSR1, sigusr1Handler);

        sigemptyset(&set);
        sigaddset(&set, SIGUSR1);

        printf("%d\n", getpid());
        while(true) 
        {
            printf(">>> ");
            if (!fgets(inputBuffer, sizeof(inputBuffer), stdin))
                break;

            if (pthread_create(&monitorThread, NULL, threadMonitor, NULL)) 
            {
                perror("Error in creating pthread\n");
                exit(1);
            }

            write(requestPipe[1], inputBuffer, MSG_SIZE);

            do
            {
                sigwaitinfo(&set, &sigInformation);
            } while (sigInformation.si_pid != childPid);

            pthread_cancel(monitorThread);
            pthread_join(monitorThread, NULL);
            char endMessage[] = "Inference Completed\n";
            fwrite(endMessage, sizeof(char), strlen(endMessage), stderr);

            //Last statement in this loop
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
    int exitStatus;

    printf("\n");
    printf("Terminating\n");
    kill(childPid, SIGINT);
    waitpid(childPid, &exitStatus, 0);
    printf("Child exited with status %d\n", exitStatus);
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
