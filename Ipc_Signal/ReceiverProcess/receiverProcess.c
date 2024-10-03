#include <stdio.h>
#include <unistd.h>

#define in_buffer_t 100



int main(int argc, char const *argv[])
{
    char inputBuffer[in_buffer_t];
    /* code */
    printf("Hello World from Receiver\n");
    while(1)
    {
        read(STDIN_FILENO, inputBuffer, in_buffer_t);
        printf("Received: %s", inputBuffer);
        fflush(stdout);
        sleep(1);
    }
    return 0;
}
