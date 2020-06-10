/*
Client program for the final project. 
Juan Francisco Gortarez A01021926
Sebastian Gonzalo Vives Faus A01025211
*/

/*
2. Mutex
3. Al inicio del combate, mandar al cliente la wave.
4. Arreglar todo el flavour text y sus sleeps
5. Balanceo de combate (we are too op)
6. Cooldown de los players

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Sockets libraries
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/poll.h>
// Signals library
#include <errno.h>
#include <signal.h>
// Custom libraries
#include "project_codes.h"
#include "sockets.c"

#define BUFFER_SIZE 1024

//--------------FUNCTION DECLARATIONS--------------------------

void usage(char *program);
void communicationLoop(int server_fd);
void onInterrupt(int signal);
void setupHandlers();
sigset_t setupMask();
//--------------------------------------------------------------

int isInterrupted = 0;

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

    // Configure the handler to catch SIGINT
    setupHandlers();
    setupMask();

    // Start the server
    connection_fd = connectSocket(argv[1], argv[2]);
    // Use the bank operations available
    communicationLoop(connection_fd);
    // Close the socket
    close(connection_fd);

    return 0;
}

/*
    Modify the signal handlers for specific events
*/
void setupHandlers()
{
    struct sigaction action;

    //clean
    bzero(&action, sizeof(action));

    //State: ignore a signal
    action.sa_handler = onInterrupt;

    //Sig fill
    sigfillset(&action.sa_mask);

    //Set the handler
    if (sigaction(SIGINT, &action, NULL) == -1)
    {
        perror("Error in setup handler");
    }
}

sigset_t setupMask()
{
    sigset_t new_set;
    sigset_t old_set;

    if (sigfillset(&new_set) == -1)
    {
        perror("Error in filled set declaration");
        exit(1);
    }

    if (sigdelset(&new_set, SIGINT) == -1)
    {
        perror("Error in removing SIGINT from blocked");
        exit(1);
    }

    if (sigprocmask(SIG_BLOCK, &new_set, &old_set) == -1)
    {
        perror("Error in sigprocmask");
        exit(1);
    }
    return old_set;
}

void onInterrupt(int signal)
{

    printf("\n Client interrupted! \n");
    isInterrupted = 1;
    //IF you want to use program data or communicate sth you need to use global variables.
}

void communicationLoop(int server_fd)
{
    char buffer[3 * BUFFER_SIZE];
    char name[BUFFER_SIZE];
    char target_name[BUFFER_SIZE];
    int health = 0;
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
    int timeout = 500;     // Time in milliseconds (0.5 seconds)
    int poll_response = 0; //Poll return value

    int loop = 1;
    int target = 0;
    int damage = 0;
    int isDead = 0;
    int counter = 0;

    //HANDSHAKE WITH SERVER
    recvData(server_fd, buffer, BUFFER_SIZE + 1);
    sscanf(buffer, "%d", &serverCode);
    if (serverCode == SYN)
    {
        printf("\nThread created and connected with server! \n");
    }
    else
    {
        loop = 0;
        isInterrupted = 1;
    }

    // CHARACTER CREATION
    //Select a name
    printf("\nPlease write your name: \n");
    scanf("%s", name);
    printf("Your name is: %s", name);

    while (loop)
    {

        //Select a class
        printf("\nSelect your class: \n");
        printf("1. Knight: A balanced choice. Low damage, high health \n ");
        printf("2. Rogue: An aggressive choice. High damage, low health \n ");
        printf("3. Barbarian: An unpredictable choice. High damage, High health, low chance to hit \n ");
        scanf("%d", &option);

        //Check if option is valid
        if (option == 1 || option == 2 || option == 3)
        {
            printf("Option chosen is: %d, sending it to server...", option);
            sprintf(buffer, "%s %d", name, option);
            sendData(server_fd, buffer, BUFFER_SIZE); //Send class to server.
            loop = 0;
        }
        else
        {
            printf("\nError! Class not found, try again.\n");
        }
    } // end while

    if (!isInterrupted)
    {
        loop = 1;
    }

    // PREPARE FOR COMBAT!
    //HANDSHAKE WITH SERVER
    recvData(server_fd, buffer, BUFFER_SIZE + 1);
    sscanf(buffer, "%d", &serverCode);
    if (serverCode == READY)
    {
        printf("\nReady for combat! \n");
    }
    else
    {
        loop = 0;
    }

    //Flavour text
    printf("You and your friends are exploring a long-forgotten dungeon.\n ");
    //sleep(3);
    printf("You haven't encountered anyone yet, but the constant jeers and cackles remind you that you're not alone here. \n");
    //sleep(4);
    printf("As you round a corner, you almost collide with a group of monsters. They don't seem happy at your intrusion! \n");
    //sleep(4);
    printf("They raise their weapons and advance on you! IT'S A FIGHT!");
    //sleep(3);

    //Start combat
    int printed = 0; //Variable to check if the menu has been printed or not.
    while (!isInterrupted && !isDead)
    {
        // POLLIN
        poll_response = poll(poll_fds, 2, timeout);

        // Nothing is recieved, we can check if interrupted.
        if (poll_response == 0)
        {
        }

        // Nothing is recieved, we can ask the user for an action

        if (poll_response > 0)
        {

            if (poll_fds[0].revents == POLLIN)
            { // Check to receive something from socket
                recvData(server_fd, buffer, BUFFER_SIZE+1);
                sscanf(buffer, "%s %d %s %d %d %d", name, &serverCode, target_name, &target, &damage, &health);
                printf("\n BUFFER: %s %d %s %d %d %d\n",  name, serverCode, target_name, target, damage, health);

                if (serverCode == ATTACK)
                {
                    printf("\n%s has struck %s for %d damage!\n%s remaining health: %d", name, target_name, damage, target_name, health);
                    printed = 0;
                }

                else if (serverCode == DEFEND)
                {
                    printf("\n%s decided to defend, reducing incoming damage to him!\n%s remaining health: %d", name, name, health);
                    printed = 0;
                }

                else if (serverCode == MISS)
                {
                    printf("\n%s tried to attack %s but missed!\n", name, target_name);
                    printed = 0;
                }

                else if (serverCode == NEWWAVE)
                {
                    printf("\nWave cleared! you gained some HP! %s remaining health: %d \n", name, health);
                    printed = 0;
                }

                else if (serverCode == VICTORY)
                {
                    printf("\n CONGRATULATIONS! YOU HAVE DEFEATED SHIVA AND SAVED THE KINGDOM!\n THANKS FOR PLAYING! OWO\n");
                    printed = 0;
                    break;
                }

                else if (serverCode == EXIT)
                {
                    //If you have been disconnected
                    printf("\nYou have died!\n");
                    break;
                }

                else
                {
                    printf("\nAction recieved unclear.\n");
                    printed = 0;
                }
            }

            else if (poll_fds[1].revents == POLLIN)
            { //If something is received from the keyboard
                scanf("%d", &option);
                printf("%d", option);

                // If player decides to attack, choose a monster to attack.
                if (option == 1)
                {
                    printf("Choose a target to attack! There are as many enemies as there are members of your party\n"); //Add list spoiler?
                    scanf("%d", &target);
                    serverCode = ATTACK;
                    sprintf(buffer, "%d %d %d", serverCode, option, target);
                    sendData(server_fd, buffer, BUFFER_SIZE);
                }
                //If the player decides to defend
                else if (option == 2)
                {
                    serverCode = DEFEND;
                    sprintf(buffer, "%d %d %d", serverCode, option, 0);
                    sendData(server_fd, buffer, BUFFER_SIZE);
                }
                else
                {
                    printf("ERROR: No valid option was chosen, try again.\n");
                    printed = 0;
                }
            }
        }

        else{
             if (isInterrupted)
            { //If my server was interrupted...
                printf("Client was interrupted! Closing connection...\n");
                //serverCode = EXIT;
                sprintf(buffer, "%d %d %d", 7, 0, 0);
                sendData(server_fd, buffer, BUFFER_SIZE);

                printf("Bye bye!");
                break;
                loop = 0;
                printed = 1;
            }

            if (!printed)
            {
                printf("\nChoose an action, hero:\n");
                printf("1. Attack!\n");
                printf("2. Defend!\n");
                printed = 1;
            }
/*
            if (isDead)
                {
                    printf("You have been killed by your enemies! Your party must continue adventuring without you.");
                    serverCode = EXIT;
                    sprintf(buffer, "%d %d %d", serverCode, 0, 0);
                    sendData(server_fd, buffer, BUFFER_SIZE);
                    break;
                }
*/
            
        }

    } // End while
}