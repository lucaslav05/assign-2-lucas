#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_LEN 256
#define CLIENT_FIFO "/tmp/client_fifo"
#define SERVER_FIFO "/tmp/server_fifo"
#define FILE_MODE 0666

void *listenClient(void *arg);
int   processFilter(const char *filter, char *msg, size_t len);
void  handleExit(int sig);

//Main function for server
int main(void)
{
    int       fifoDescriptor;
    pthread_t listenerThread;
    signal(SIGINT, handleExit);

    // Create FIFOs
    mkfifo(CLIENT_FIFO, FILE_MODE);
    mkfifo(SERVER_FIFO, FILE_MODE);

    while(1)
    {
        fifoDescriptor = open(CLIENT_FIFO, O_RDONLY | O_CLOEXEC);
        if((fifoDescriptor == -1) & (SIGINT == 2))
        {
            printf("\nServer shutting down..\n");
            return EXIT_SUCCESS;
        }
        if(fifoDescriptor == -1)
        {
            fprintf(stderr, "Error: Could not open client fifo\n");
            return EXIT_FAILURE;
        }

        // Create thread to listen to the client
        if(pthread_create(&listenerThread, NULL, listenClient, (void *)&fifoDescriptor) != 0)
        {
            fprintf(stderr, "Error: Could not create thread\n");
            goto error_cleanup;
        }

        // Wait for the listener thread to finish
        pthread_join(listenerThread, NULL);
        close(fifoDescriptor);
    }

    // Error cleanup
error_cleanup:
    close(fifoDescriptor);
    unlink(CLIENT_FIFO);
    unlink(SERVER_FIFO);
    return EXIT_FAILURE;
}

void handleExit(int sig)
{
    (void)sig;
    unlink(CLIENT_FIFO);
    unlink(SERVER_FIFO);
}

int processFilter(const char *filter, char *msg, size_t len)
{
    if(filter == NULL)
    {
        fprintf(stderr, "Error: Empty filter\n");
        return -1;
    }

    // Convert to uppercase
    if(strcmp(filter, "upper") == 0)
    {
        for(size_t i = 0; i < len && msg[i] != '\0'; i++)
        {
            msg[i] = (char)toupper((unsigned char)msg[i]);
        }
        return 0;
    }

    // Convert to lowercase
    if(strcmp(filter, "lower") == 0)
    {
        for(size_t i = 0; i < len && msg[i] != '\0'; i++)
        {
            msg[i] = (char)tolower((unsigned char)msg[i]);
        }
        return 0;
    }

    // No filter
    if(strcmp(filter, "none") == 0)
    {
        return 0;
    }

    // Invalid filter
    return -1;
}

void *listenClient(void *arg)
{
    int  clientFile;
    int  serverFile = *((int *)arg);
    char readBuffer[BUFFER_LEN];

    const char *filterOption;
    char       *msgContent;
    char       *tokenState;
    ssize_t     bytesRead = read(serverFile, readBuffer, BUFFER_LEN - 1);
    if(bytesRead == -1)
    {
        fprintf(stderr, "Error: Failed to read from fifo\n");
        pthread_exit(NULL);
    }

    // Null-terminate the buffer
    readBuffer[bytesRead] = '\0';

    // Open server FIFO
    clientFile = open(SERVER_FIFO, O_WRONLY | O_CLOEXEC);
    if(clientFile == -1)
    {
        fprintf(stderr, "Error opening fifo\n");
        pthread_exit(NULL);
    }

    // Parse the filter and message
    filterOption = strtok_r(readBuffer, "\n", &tokenState);
    msgContent   = strtok_r(NULL, "\n", &tokenState);

    // Apply filter
    if(processFilter(filterOption, msgContent, BUFFER_LEN) == -1)
    {
        fprintf(stderr, "Error: Invalid filter\n");
        close(clientFile);
        pthread_exit(NULL);
    }

    // Write the processed message back
    if(write(clientFile, msgContent, BUFFER_LEN - 1) == -1)
    {
        fprintf(stderr, "Error writing to server FIFO\n");
        close(clientFile);
        pthread_exit(NULL);
    }

    close(clientFile);
    return NULL;
}
