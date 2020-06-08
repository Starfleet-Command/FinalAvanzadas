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
    char buffer[4*BUFFER_SIZE];
    char name[BUFFER_SIZE];
    int option;
    int class;
    int serverCode = -1;

    struct pollfd poll_fds[2];
    // Fill in the structure
    poll_fds[0].fd = server_fd; //socket
    poll_fds[0].events = POLLIN;

    poll_fds[1].fd = 0; //standard input (keyboard)
    poll_fds[1].events = POLLIN;

    // Pollin
    int timeout = 500; // Time in milliseconds (0.5 seconds)
    int poll_response = 0;    //Poll return value

    int loop = 1;
    int target = 0;
    int damage = 0;

    int counter =0;

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

    loop = 1;
    // PREPARE FOR COMBAT!
    //HANDSHAKE WITH SERVER
    recvData(server_fd, buffer, BUFFER_SIZE + 1);
    sscanf(buffer, "%d", &serverCode);
    if(serverCode == READY){
        printf("\nReady for combat! \n");
    }

    //Start combat
    int printed = 0; //Variable to check if the menu has been printed or not.
    while (loop){
        // POLLIN
        poll_response = poll(poll_fds, 2, timeout);

        // Nothing is recieved, we can ask the user for an action
            if(!printed){
                printf("\nChoose an action, hero:\n");
                printf("1. Attack!\n");
                printf("2. Defend!\n");
                printed=1;
            }

        if(poll_response > 0){
            
            // Tengo 2 file descriptors, a ver cual me escribio
            if(poll_fds[0].revents == POLLIN){ // Si recibo algo del teclado
                recvData(server_fd, buffer, BUFFER_SIZE + 1);
                        sscanf(buffer, "%s %d %d %d", name, &serverCode, &target, &damage);
                        printf("\nAction is: %d\n", serverCode);

                        if(serverCode == ATTACK){
                            printf("\n%s has struck monster %d for %d damage!\n", name, target, damage);
                        }

                        else if(serverCode == DEFEND){
                            printf("\n%s decided to defend, reducing incoming damage to him!\n", name);
                        }

                        else{
                            printf("\nAction recieved unclear.\n");
                            
                        }

                        printed=0;
            }

            else if(poll_fds[1].revents == POLLIN){ //Si recibo algo del socket
                scanf("%d", &option);

                // If player decides to attack, choose a monster to attack.
                if(option == 1){
                    printf("Choose a target to attack! (EXPERIMENTAL, CHOOSE ANY NUMBER)\n");
                    scanf("%d", &target);
                    serverCode = ATTACK;
                    sprintf(buffer, "%d %d %d", serverCode, option, target);
                    sendData(server_fd, buffer, BUFFER_SIZE);

                }
                //If the player decides to defend
                else if(option == 2){
                    serverCode = DEFEND;
                    sprintf(buffer, "%d %d %d", serverCode, option, 0);
                    sendData(server_fd, buffer, BUFFER_SIZE);

                }
                else{
                    printf("ERROR: No valid option was chosen, try again.\n");
                    printed=0;
                }
            }


        }
        
    }
    

    sleep(5);
    printf("Bye bye!");

}