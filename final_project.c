/*
    Final project program
    An AFK clicker-style game with linear progression and classes. 

    Juan Francisco Gortarez Ricardez A01021926
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Signals library
#include <errno.h>
#include <signal.h>
// Sockets libraries
#include <netdb.h>
#include <sys/poll.h>
// Posix threads library
#include <pthread.h>
#include "project_codes.h"
#include "sockets.c"

#define MIN_CLIENTS 3
#define MAX_BUFSIZ 1024
#define MAX_QUEUE 7
#define MAX_PLAYERS 5

typedef struct player_struct //Basic information that other players should know. More detailed info is handled server-side only.
{
    char *name;
    int class;
    int hp;
    int cooldown;
    int damage;
    float ctH;
} player_t;

typedef struct thread_info //Information sent to each thread about other processes
{
    player_t *players;
    int game_status; // 0 is OK, 1 is Waiting, -1 is interrupted.
    int connection_fd;
    int playerno;
} thread_t;

int isInterrupted;

//--------------FUNCTION DECLARATIONS--------------------------

void usage(char *program);
void setupHandlers();
sigset_t setupMask();
int recvString(int connection_fd, char *buffer, int size);
void sendString(int connection_fd, char *buffer);
void waitForConnections(int server_fd, thread_t *threadinfo);
void onInterrupt(int signal);
void *gameLoop(void *arg);

//--------------------------------------------------------------

int main(int argc, char *argv[])
{
    int server_fd;
    thread_t threadData;

    //Initialize the thread_t struct
    threadData.players = malloc(MAX_PLAYERS * sizeof(player_t));
    threadData.game_status = 0;

    printf("\n=== WELCOME TO THE THREAD WARRIORS ===\n");

    // Check the correct arguments
    if (argc != 2)
    {
        usage(argv[0]);
    }

    // Configure the handler to catch SIGINT
    setupHandlers();
    setupMask();

    // Initialize the data structures

    // Show the IPs assigned to this computer
    printLocalIPs();
    // Start the server
    server_fd = initServer(argv[1], MAX_QUEUE);
    // Listen for connections from the clients
    waitForConnections(server_fd, &threadData);
    // Close the socket
    close(server_fd);

    // Finish the main thread
    pthread_exit(NULL);

    return 0;
}

void usage(char *program)
{
    printf("Usage:\n");
    printf("\t%s {port_number}\n", program);
    exit(EXIT_FAILURE);
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

int recvString(int connection_fd, char *buffer, int size)
{
    int chars_read;
    // Clear the buffer
    bzero(buffer, size);

    // Read the request from the client
    chars_read = recv(connection_fd, buffer, size, 0);
    // Error when reading
    if (chars_read == -1)
    {
        perror("ERROR: recv");
    }
    // Connection finished
    if (chars_read == 0)
    {
        printf("Connection disconnected\n");
        return 0;
    }

    return 1;
}

//Declaration of function from sockets.h
void sendString(int connection_fd, char *buffer)
{
    struct pollfd poll_list[1];
    poll_list[0].fd = connection_fd;
    poll_list[0].events = POLLOUT;
    int timeout = 500;

    int retval;
    retval = poll(poll_list, 1, timeout);

    if (retval > 0)
    {

        if (poll_list[0].revents & POLLOUT)
        {
            // Send a message to the client, including an extra character for the '\0'
            if (send(connection_fd, buffer, strlen(buffer) + 1, 0) == -1)
            {
                perror("ERROR: send");
            }
        }

        if (poll_list[0].revents & POLLHUP)
        {
            perror("ERROR:Disconnected");
        }
    }
}

void onInterrupt(int signal)
{

    isInterrupted = 1;
    //IF you want to use program data or communicate sth you need to use global variables.
}

void waitForConnections(int server_fd, thread_t *threadinfo)
{
    struct sockaddr_in client_address;
    socklen_t client_address_size;
    char client_presentation[INET_ADDRSTRLEN];
    int client_fd;
    int timeout = 500; // Time in milliseconds (0.5 seconds)
    int retval = 0;    //Poll return value
    int playerNo = 0;

    // Create a structure array to hold the file descriptors to poll
    struct pollfd test_fds[1];
    // Fill in the structure
    test_fds[0].fd = server_fd;
    test_fds[0].events = POLLIN; // Check for incomming data

    // Get the size of the structure to store client information
    client_address_size = sizeof client_address;

    while (!isInterrupted)
    {
        player_t player;
        pthread_t new_tid;
        // ACCEPT
        // Wait for a client connection
        retval = poll(test_fds, 1, timeout);
        if (retval > 0)
        {
            if (test_fds[0].revents & POLLIN && playerNo < 5)
            {
                client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_size);
                if (client_fd == -1)
                {
                    perror("ERROR: accept");
                }

                // Get the data from the client
                inet_ntop(client_address.sin_family, &client_address.sin_addr, client_presentation, sizeof client_presentation);
                printf("Received incoming connection from %s on port %d\n", client_presentation, client_address.sin_port);

                //Put the appropriate info to send to thread
                threadinfo->connection_fd = client_fd;
                threadinfo->players[playerNo] = player;
                threadinfo->playerno = playerNo;

                // CREATE A THREAD
                if (pthread_create(&new_tid, NULL, &gameLoop, threadinfo) == -1)
                {
                    perror("Error creating a thread");
                }
            }
        }
    }
}

void *gameLoop(void *arg)
{
    thread_t *threadData;
    int client_fd;
    threadData = (thread_t *)arg;
    client_fd = threadData->connection_fd;
    int playerno = threadData->playerno;
    char buffer[MAX_BUFSIZ];
    int isReady = 0;
    // Create a structure array to hold the file descriptors to poll
    struct pollfd poll_list[1];
    // Fill in the structure
    poll_list[0].fd = client_fd;
    poll_list[0].events = POLLIN; // Check for incomming data
    int retval = 0;

    sprintf(buffer, "%d", SYN);
    sendString(client_fd, buffer); //Ensure client is there and await name and class.

    while (!isReady)
    {
        if (retval > 0)
        {

            if (poll_list[0].revents & POLLIN) //Poll to receive answer
            {
                // Send a message to the client, including an extra character for the '\0'
                if (recvString(client_fd, buffer, strlen(buffer) + 1) == -1)
                {
                    perror("ERROR: receive");
                }
                isReady = 1;
                sscanf(buffer, "%s %d", threadData->players[playerno].name, &threadData->players[playerno].class); //Receive name and class selection from client.
            }
        }
    }
}
