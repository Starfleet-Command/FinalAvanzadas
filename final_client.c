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
#include "project_codes.h"
#include "sockets.c"

#define BUFFER_SIZE 1024

//--------------FUNCTION DECLARATIONS--------------------------

void usage(char *program);
void communicationLoop(int server_fd);
//--------------------------------------------------------------

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
    char buffer[BUFFER_SIZE];
    char name[BUFFER_SIZE];
    char option;
    int class;
    int serverCode = -1;
    int retval = 0;
    int timeout = 500;

    struct pollfd poll_fds[1];
    // Fill in the structure
    poll_fds[0].fd = server_fd;
    poll_fds[0].events = POLLIN;

    while (serverCode != SYN) //Wait for server SYNCHRONIZE
    {
        retval = poll(poll_fds, 1, timeout);
        if (retval > 0)
        {
            if (poll_fds[0].revents & POLLIN)
            {
                recvData(server_fd, buffer, BUFFER_SIZE + 1);
                sscanf(buffer, "%d", &serverCode);
            }
        }
    }

    printf("Please write your name: \n");
    scanf("%s", name);
    getchar();
    printf("Select your class: \n");
    printf("1. Knight: A balanced choice. Low damage, high health \n ");
    printf("2. Rogue: An aggressive choice. High damage, low health \n ");
    printf("3. Barbarian: An unpredictable choice. High damage, High health, low chance to hit \n ");
    scanf("%c", &option);
    class = (int)option - 1; //Potentially dangerous? Sanitize inputs?
    sprintf(buffer, "%s %d", name, class);
    sendData(server_fd, buffer, BUFFER_SIZE); //Send class to server.

    while (serverCode != UPDATE || serverCode != EXIT)
    {
        retval = poll(poll_fds, 1, timeout);
        if (retval > 0)
        {
            if (poll_fds[0].revents & POLLIN)
            {
                recvData(server_fd, buffer, BUFFER_SIZE + 1);
                sscanf(buffer, "%d", &serverCode);
            }
        }
    }
}