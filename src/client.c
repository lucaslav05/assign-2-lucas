#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CLIENT_FIFO "/tmp/client_fifo"
#define SERVER_FIFO "/tmp/server_fifo"
#define MAX_FILTER_LEN 10
#define MAX_MSG_LEN 256

void *sendToFIFO(void *arg);

// Scruct containing thread data
typedef struct
{
    int  fileDesc;
    char filterOption[MAX_FILTER_LEN];
    char userMsg[MAX_MSG_LEN];
} thread_data;

// Main function for client
int main(int argc, char *argv[])
{
    int       clientFile;
    int       serverFile;
    char      serverResponse[MAX_MSG_LEN];
    ssize_t   bytesReceived;
    pthread_t sendThread;

    int         option;
    const char *userFilter;
    const char *msgContent;

    thread_data thread_args;
    userFilter = NULL;
    msgContent = NULL;

    while((option = getopt(argc, argv, "f:m:")) != -1)
    {
        switch(option)
        {
            case 'f':
                userFilter = optarg;
                break;
            case 'm':
                msgContent = optarg;
                break;
            default:
                fprintf(stderr, "Usage: -f filter -m message\n");
                return EXIT_FAILURE;
        }
    }

    if(userFilter == NULL || msgContent == NULL)
    {
        fprintf(stderr, "Error: Filter and message must be provided\n");
        return EXIT_FAILURE;
    }

    // Open FIFO for writing
    clientFile = open(CLIENT_FIFO, O_WRONLY | O_CLOEXEC);
    if(clientFile == -1)
    {
        perror("Error opening client FIFO for writing");
        return EXIT_FAILURE;
    }

    // Populate the thread data
    thread_args.fileDesc = clientFile;
    strncpy(thread_args.filterOption, userFilter, sizeof(thread_args.filterOption) - 1);
    strncpy(thread_args.userMsg, msgContent, sizeof(thread_args.userMsg) - 1);

    // Create thread to write to FIFO
    if(pthread_create(&sendThread, NULL, sendToFIFO, (void *)&thread_args) != 0)
    {
        perror("Error creating writer thread");
        close(clientFile);
        return EXIT_FAILURE;
    }

    // Wait for the thread to complete
    pthread_join(sendThread, NULL);

    // Open FIFO to read server response
    serverFile = open(SERVER_FIFO, O_RDONLY | O_CLOEXEC);
    if(serverFile == -1)
    {
        fprintf(stderr, "Error: Unable to read from server FIFO\n");
        close(clientFile);
        return EXIT_FAILURE;
    }

    bytesReceived = read(serverFile, serverResponse, MAX_MSG_LEN - 1);
    if(bytesReceived == -1)
    {
        fprintf(stderr, "Error: Failed to read from server\n");
        goto cleanup;
    }

    printf("%s\n", serverResponse);
    close(clientFile);
    close(serverFile);
    return EXIT_SUCCESS;

cleanup:
    close(clientFile);
    close(serverFile);
    return EXIT_FAILURE;
}

// Thread function to write to FIFO
void *sendToFIFO(void *arg)
{
    const thread_data *params = (thread_data *)arg;

    int  clientFile = params->fileDesc;
    char buffer[MAX_MSG_LEN];

    size_t totalSize = (size_t)snprintf(NULL, 0, "%s\n%s", params->filterOption, params->userMsg) + 1;
    snprintf(buffer, totalSize, "%s\n%s", params->filterOption, params->userMsg);

    // Write to FIFO
    if(write(clientFile, buffer, strlen(buffer)) == -1)
    {
        fprintf(stderr, "Error: Failed to send data to server\n");
        close(clientFile);
        pthread_exit(NULL);
    }

    // Close the file descriptor
    close(clientFile);
    return NULL;
}
