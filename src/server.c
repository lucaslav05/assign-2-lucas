#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUF_SIZE 256
#define CLIENT_PATH "/home/lucas-laviolette/tmp/client_fifo"
#define SERVER_PATH "/home/lucas-laviolette/tmp/server_fifo"
#define PERMISSION 0666

void *start_listening(void *arg);
int   apply_filter(const char *filter, char *msg, size_t msg_size);
void  cleanup(int sig);

int main(void)
{
    int       fd;
    pthread_t listenerThread;
    signal(SIGINT, cleanup);

    // Make FIFO's
    mkfifo(CLIENT_PATH, PERMISSION);
    mkfifo(SERVER_PATH, PERMISSION);

    while(1)
    {
        // Open the file descriptor to read only
        fd = open(CLIENT_PATH, O_RDONLY | O_CLOEXEC);
        if((fd == -1) & (SIGINT == 2))
        {
            printf("\nClosing server and exiting program\n");
            return EXIT_SUCCESS;
        }
        if(fd == -1)
        {
            fprintf(stderr, "Error: Could not open client FIFO on server end\n");
            return EXIT_FAILURE;
        }

        // Create a thread to start listening for incoming messages
        if(pthread_create(&listenerThread, NULL, start_listening, (void *)&fd) != 0)
        {
            fprintf(stderr, "Error: could not creating thread\n");
            goto cleanup;
        }

        // Wait for the listener thread to finish
        pthread_join(listenerThread, NULL);
        close(fd);
    }

    // Cleanup upon error
cleanup:
    close(fd);
    unlink(CLIENT_PATH);
    unlink(SERVER_PATH);
    return EXIT_FAILURE;
}

void cleanup(int sig)
{
    (void)sig;
    unlink(CLIENT_PATH);
    unlink(SERVER_PATH);
}

int apply_filter(const char *filter, char *msg, size_t msg_size)
{
    if(filter == NULL)
    {
        fprintf(stderr, "Error: Filter is NULL\n");
        return -1;
    }

    // upper
    if(strcmp(filter, "upper") == 0)
    {
        for(size_t i = 0; i < msg_size && msg[i] != '\0'; i++)
        {
            msg[i] = (char)toupper((unsigned char)msg[i]);
        }
        return 0;
    }

    // lower
    if(strcmp(filter, "lower") == 0)
    {
        for(size_t i = 0; i < msg_size && msg[i] != '\0'; i++)
        {
            msg[i] = (char)tolower((unsigned char)msg[i]);
        }
        return 0;
    }

    // none
    if(strcmp(filter, "none") == 0)
    {
        return 0;
    }

    // error case
    return -1;
}

void *start_listening(void *arg)
{
    int  serverfd;
    int  clientfd = *((int *)arg);    // Cast and dereference the argument
    char buff[BUF_SIZE];

    const char *filter;
    char       *message;
    char       *state;
    ssize_t     bytesRead = read(clientfd, buff, BUF_SIZE - 1);
    if(bytesRead == -1)
    {
        fprintf(stderr, "Error: couldn't read from FIFO");
        pthread_exit(NULL);
    }

    // Null terminate the end of the buffer
    buff[bytesRead] = '\0';

    // Open the server fd
    serverfd = open(SERVER_PATH, O_WRONLY | O_CLOEXEC);
    if(serverfd == -1)
    {
        fprintf(stderr, "Error opening server file descriptor\n");
        pthread_exit(NULL);
    }

    // Split the argument and the message
    filter  = strtok_r(buff, "\n", &state);
    message = strtok_r(NULL, "\n", &state);

    // Compare the strings and apply the filter
    if(apply_filter(filter, message, BUF_SIZE) == -1)
    {
        fprintf(stderr, "Error: Filter is invalid");
        close(serverfd);
        pthread_exit(NULL);
    }

    // Write back to the other server
    if(write(serverfd, message, BUF_SIZE - 1) == -1)
    {
        fprintf(stderr, "Error trying to write to server FIFO");
        close(serverfd);
        pthread_exit(NULL);
    }

    close(serverfd);
    return NULL;
}
