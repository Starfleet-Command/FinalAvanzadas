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

    printf("\n=== FINAL PROJECT CLIENT PROGRAM ===\n Wait for all other players to join...\n");

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
    int option;
    int class;
    int serverCode = -1;
    int retval = 0;
    int timeout = 500;

    struct pollfd poll_fds[1];
    // Fill in the structure
    poll_fds[0].fd = server_fd;
    poll_fds[0].events = POLLIN;

    int loop = 1;

    //HANDSHAKE WITH SERVER
    recvData(server_fd, buffer, BUFFER_SIZE + 1);
    sscanf(buffer, "%d", &serverCode);
    if(serverCode == SYN){
        printf("\nThread created and connected with server! \n");
    }

    // CHARACTER CREATION
    //Select a name
    printf("\nPlease write your name: \n");
    scanf("%s", name);
    printf("Your name is: %s", name);

    while(loop){

        //Select a class
        printf("\nSelect your class: \n");
        printf("1. Knight: A balanced choice. Low damage, high health \n ");
        printf("2. Rogue: An aggressive choice. High damage, low health \n ");
        printf("3. Barbarian: An unpredictable choice. High damage, High health, low chance to hit \n ");
        scanf("%d", &option);
        
        //Check if option is valid
        if(option == 1 || option == 2 || option == 3){
            printf("Option chosen is: %d, sending it to server...", option);
            sprintf(buffer, "%s %d", name, option);
            sendData(server_fd, buffer, BUFFER_SIZE); //Send class to server.
            loop=0;
        }
        else{
            printf("\nError! Class not found, try again.\n");
        }
    } // end while
    sleep(5);
    printf("Bye bye!");
    /*

    
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
    */
}