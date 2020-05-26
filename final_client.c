/*
Client program for the final project. 
Juan Francisco Gortarez A01021926
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Sockets libraries
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/poll.h>
// Custom libraries
#include "bank_codes.h"
#include "sockets.c"

#define BUFFER_SIZE 1024

void usage(char *program)
{
    printf("Usage:\n");
    printf("\t%s {server_address} {port_number}\n", program);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    int connection_fd;

    printf("\n=== FINAL PROJECT CLIENT PROGRAM ===\n");

    // Check the correct arguments
    if (argc != 3)
    {
        usage(argv[0]);
    }

    // Start the server
    connection_fd = connectSocket(argv[1], argv[2]);
    // Use the bank operations available
    communicationLoop(connection_fd);
    // Close the socket
    close(connection_fd);

    return 0;
}

void communicationLoop(int server_fd)
{
    char *buffer[BUFFER_SIZE];
    int serverCode = -1;
    int retval = 0;
    int timeout = 500;

    struct pollfd poll_fds[1];
    // Fill in the structure
    poll_fds[0].fd = server_fd;
    poll_fds[0].events = POLLIN;

    while (serverCode != 0) //Wait for server ACKNOWLEDGE
    {
        retval = poll(test_fds, 1, timeout);
        if (retval > 0)
        {
            if (test_fds[0].revents & POLLIN)
            {
                recvData(server_fd, buffer, BUFFER_SIZE + 1);
                sscanf(buffer, "%d", serverCode);
            }
        }
    }