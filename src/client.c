#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CLIENT_PATH "/home/lucas-laviolette/tmp/client_fifo"
#define SERVER_PATH "/home/lucas-laviolette/tmp/server_fifo"
#define FILTER_SIZE 10
#define MSG_SIZE 256

void *writeToFIFO(void *arg);

// All arguments that need to be passed into the thread function
typedef struct
{
    int  fd;
    char filter[FILTER_SIZE];
    char message[MSG_SIZE];
} thread_args;

// Start of program
int main(int argc, char *argv[])
{
    int       clientfd;
    int       serverfd;
    char      incomingMsg[MSG_SIZE];
    ssize_t   bytesRead;
    pthread_t writerThread;

    // Variables for input
    int         opt;
    const char *filter;
    const char *message;

    thread_args thread_data;
    filter  = NULL;
    message = NULL;
    while((opt = getopt(argc, argv, "f:m:")) != -1)
    {
        switch(opt)
        {
            case 'f':
                filter = optarg;
                break;
            case 'm':
                message = optarg;
                break;
            default:
                fprintf(stderr, "Usage: -a argument -m message\n");
                return EXIT_FAILURE;
        }
    }

    if(filter == NULL || message == NULL)
    {
        fprintf(stderr, "Error: filter or message cannot be null\n");
        return EXIT_FAILURE;
    }

    // Open the fd for writing to the FIFO. Blocks if server is not on
    clientfd = open(CLIENT_PATH, O_WRONLY | O_CLOEXEC);
    if(clientfd == -1)
    {
        perror("Error opening FIFO for writing");
        return EXIT_FAILURE;
    }

    // Create the data struct and fill
    thread_data.fd = clientfd;
    strncpy(thread_data.filter, filter, sizeof(thread_data.filter) - 1);
    strncpy(thread_data.message, message, sizeof(thread_data.message) - 1);

    // Create a new thread to write to the FIFO
    if(pthread_create(&writerThread, NULL, writeToFIFO, (void *)&thread_data) != 0)
    {
        perror("Error creating thread\n");
        close(clientfd);
        return EXIT_FAILURE;
    }

    // Wait for the thread to finish
    pthread_join(writerThread, NULL);

    // Wait for server response
    serverfd = open(SERVER_PATH, O_RDONLY | O_CLOEXEC);
    if(serverfd == -1)
    {
        fprintf(stderr, "Error: couldn't read bytes\n");
        close(clientfd);
        return EXIT_FAILURE;
    }

    bytesRead = read(serverfd, incomingMsg, MSG_SIZE - 1);
    if(bytesRead == -1)
    {
        fprintf(stderr, "Error: couldn't read bytes\n");
        goto cleanup;
    }

    printf("%s\n", incomingMsg);
    close(clientfd);
    close(serverfd);
    return EXIT_SUCCESS;

cleanup:
    close(clientfd);
    close(serverfd);
    return EXIT_FAILURE;
}

// Thread function to write to FIFO
void *writeToFIFO(void *arg)
{
    const thread_args *data = (thread_args *)arg;

    int  clientfd = data->fd;
    char msgBuffer[MSG_SIZE];

    size_t required_size = (size_t)snprintf(NULL, 0, "%s\n%s", data->filter, data->message) + 1;
    snprintf(msgBuffer, required_size, "%s\n%s", data->filter, data->message);

    // Write message to FIFO
    if(write(clientfd, msgBuffer, strlen(msgBuffer)) == -1)
    {
        fprintf(stderr, "Error: Could not write to the server\n");
        close(clientfd);
        pthread_exit(NULL);
    }

    // Close the file descriptor
    close(clientfd);
    return NULL;
}
