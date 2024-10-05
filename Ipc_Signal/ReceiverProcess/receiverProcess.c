#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h> 
#include <signal.h>
#include <stdbool.h>

#define BUFF_SIZE 257

pid_t ppid;

void sigintHandler(int signum);

void swap(int* xp, int* yp){
    int temp = *xp;
    *xp = *yp;
    *yp = temp;
}

// An optimized version of Bubble Sort
void bubbleSort(int arr[], int n){
    int i, j;
    bool swapped;
    for (i = 0; i < n - 1; i++) {
        swapped = false;
        for (j = 0; j < n - i - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                swap(&arr[j], &arr[j + 1]);
                swapped = true;
            }
        }

        // If no two elements were swapped by inner loop,
        // then break
        if (swapped == false)
            break;
    }
}

int main(int argc, char const *argv[])
{
    char inputBuffer[BUFF_SIZE];

    

    signal(SIGINT, sigintHandler);

    ppid = getppid();
    while(1)
    {
        read(STDIN_FILENO, inputBuffer, BUFF_SIZE);

        /*
            This segment is used to see if the main process can correctly
            measures the CPU usage of this process
        */
        int *arrayTestCPUUsage = malloc(sizeof(int) * 100000);
        int * p = arrayTestCPUUsage;
        for(int i=0; i<100000;i++)
            *(++p)=100000-i;
        bubbleSort(arrayTestCPUUsage, 100000);

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

