/*
    Final project program
    An AFK clicker-style game with linear progression and classes. 

    Juan Francisco Gortarez Ricardez A01021926
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
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
#define MAX_BUFSIZE 1024
#define MAX_QUEUE 7
#define MAX_PLAYERS 5

typedef struct player_struct //Basic information that other players should know. More detailed info is handled server-side only.
{
    char *name;
    int class;
    int hp;
    int cooldown;
    int damage;
    double ctH;
    int connection_fd;
    int is_defending;
} player_t;

typedef struct thread_info //Information sent to each thread about other processes
{
    player_t *players;
    int connection_fd;
    player_t *wave;
    int playerno;
    int *ready_for_combat;
    int *max_players;
} thread_t;

int isInterrupted = 0;

//--------------FUNCTION DECLARATIONS--------------------------

void usage(char *program);
void setupHandlers();
sigset_t setupMask();
int recvString(int connection_fd, char *buffer, int size);
void sendString(int connection_fd, char *buffer);
void waitForConnections(int server_fd, int max_players);
void onInterrupt(int signal);
void *waitroomLoop(void *arg);
player_t *initEntity(int class, char *name, int hp, int cd, int damage, double ctH);
void *monsterThread(void *arg);
player_t *initWave(player_t *monsters, int playerNo);
player_t *initMonsters();
int hit_or_miss(double ctH);
int choose_player(player_t *players, int max_players);
//--------------------------------------------------------------

int main(int argc, char *argv[])
{
    int server_fd;
    int max_players = atoi(argv[2]);
    srand(time(NULL));

    //Initialize the thread_t struct

    printf("\n=== WELCOME TO THE THREAD WARRIORS ===\n");

    // Check the correct arguments
    if (argc != 3)
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
    waitForConnections(server_fd, max_players);
    // Close the socket
    close(server_fd);

    // Finish the main thread
    pthread_exit(NULL);

    return 0;
}

void usage(char *program)
{
    printf("Usage:\n");
    printf("\t%s {port_number}", program);
    printf("\t <number of players>\n");
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

// QUITAR

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

void waitForConnections(int server_fd, int max_players)
{
    thread_t *threadinfo = malloc(sizeof(thread_t)); //Shared memory space for all threads to use
    int *ready_for_combat = malloc(sizeof(int));
    int *t_max_players = malloc(sizeof(int));
    *(t_max_players) = max_players;
    *(ready_for_combat) = 0;
    struct sockaddr_in client_address;
    socklen_t client_address_size;
    char client_presentation[INET_ADDRSTRLEN];
    int client_fd;
    int *client_fds = malloc(sizeof(int) * MAX_PLAYERS);
    // Pollin
    int timeout = 500; // Time in milliseconds (0.5 seconds)
    int poll_response = 0;    //Poll return value

    int playerNo = 0;
    int loop = 1;
    int createWarrior = 1;
    pthread_t new_tid;

    // Create a structure array to hold the file descriptors to poll
    struct pollfd test_fds[1];
    // Fill in the structure
    test_fds[0].fd = server_fd;
    test_fds[0].events = POLLIN; // Check for incomming data

    // Get the size of the structure to store client information
    client_address_size = sizeof client_address;

    //Monsters!
    //The threads to handle monsters, the combat system and so on should probably go here.
    player_t *monsters;
    monsters = initMonsters();

    while(loop){

        // POLLIN
        poll_response = poll(test_fds, 1, timeout);

        // Nothing is recieved, we can check if interrupted.
        if(poll_response == 0){

            if(isInterrupted){ //If my server was interrupted...
                printf("Server was interrupted! Closing server...\n");
                loop = 0;
            }

            //There is 3 players, we are not expecting nothing else, we can create the warrios...
            else if(playerNo == max_players && createWarrior){
                //Create player threads
                printf("All players ready! Creating warrios...\n");
                player_t *players = malloc(sizeof(player_t) * playerNo); //Allocate memory for player array
                threadinfo->players = players;
                threadinfo->ready_for_combat = ready_for_combat;
                threadinfo->max_players = t_max_players;

                for (int i = 0; i < max_players; i++){
                    //Put the appropriate info to send to thread
                    threadinfo->connection_fd = client_fds[i];
                    threadinfo->playerno = i;

                    // CREATE A THREAD
                    int status = pthread_create(&new_tid, NULL, &waitroomLoop, threadinfo);
                    printf("Created warrior %d with status %d\n", i, status);
                    // Check if the thread was created correctly
                    if(status == -1){
                        // The thread was NOT created correctly.
                        perror("ERROR: pthread_create");
                        // Close the client connection
                        close(client_fd);
                    } 
                } // end for

                //Create monster thread
                    int status = pthread_create(&new_tid, NULL, &monsterThread, threadinfo);
                    if(status == -1){
                        // The thread was NOT created correctly.
                        perror("ERROR: pthread_create");
                        // Close the client connection
                        close(client_fd);
                    } 

                createWarrior = 0; //end teh warrior creation process.
            }
        } // end if

        // There is something coming in, which most probably is a new connection from a client, recieve it.
        else if(poll_response == 1){

            //There is less than max_players players
            if(playerNo < max_players && createWarrior){
                // Accept new connection
                client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_size);
                // Check if connection is valid
                if(client_fd == -1){
                    perror("ERROR: accept");
                }
                else{
                    client_fds[playerNo] = client_fd; //Add to client fd array.
                    playerNo++;
                    // Get the data from the client
                    inet_ntop(client_address.sin_family, &client_address.sin_addr, client_presentation, sizeof client_presentation);
                    printf("Received incoming connection from %s on port %d\n Remaining players... %d\n", client_presentation, client_address.sin_port, 3-playerNo);

                }
            } // end if

        } // end else if

         //Else, I got an error
        else{
            perror("ERROR: poll");
        }
    }

    //free(monsters);
}

void *waitroomLoop(void *arg)
{
    thread_t *threadData;
    int client_fd;
    player_t *player;
    int class = 1;
    int action = 0;
    int target = 0;
    int serverCode;
    threadData = (thread_t *)arg;
    client_fd = threadData->connection_fd;
    int playerno = threadData->playerno;
    char buffer[3*MAX_BUFSIZE];
    char name[MAX_BUFSIZE];
    int isReady = 0;

    // Create a structure array to hold the file descriptors to poll
    struct pollfd poll_list[1];
    // Fill in the structure
    poll_list[0].fd = client_fd;
    poll_list[0].events = POLLIN; // Check for incomming data
     // Pollin
    int timeout = 500; // Time in milliseconds (0.5 seconds)
    int poll_response = 0;    //Poll return value

    int loop = 1;

    // HANDSHAKE WITH CLIENT
    // SEND
    sprintf(buffer, "%d", SYN);
    sendString(client_fd, buffer); //Ensure client is there and await name and class.

    while(loop && !isInterrupted){
        // POLLIN
        poll_response = poll(poll_list, 1, timeout);

        // Nothing is recieved, we can check if interrupted.
        if(poll_response == 0){
            if(isInterrupted){ //If my server was interrupted...
                printf("Thread was interrupted! Closing thread...\n");
                break;
            }
        }

        // There is something coming in, which most probably is the information from the client.
        else if(poll_response == 1){
             // RECIEVE CLASS AND NAME FROM CLIENT
             recvString(client_fd, buffer, MAX_BUFSIZE+1);
             sscanf(buffer, "%s %d", name, &class); //Receive name and class selection from client.
             printf("\nRecieved data!\n");

             threadData->players[playerno].class = class;
             threadData->players[playerno].name = name;

             switch (class) //Filling in the character info based on selected class
                {
                case 1:
                    player = initEntity(0, name, 250, 5, 25, 0.80); //Warrior
                    printf("Created a Warrior!\n");
                    break;

                case 2:
                    player = initEntity(1, name, 150, 3, 40, 0.90); //Rogue
                    printf("Created a Rogue!\n");
                    break;

                case 3:
                    player = initEntity(2, name, 250, 7, 50, 0.65); //Barbarian
                    printf("Created a Barbarian!\n");
                    break;

                default: //Should never reach here
                    break;
                }
            player->connection_fd = client_fd;
            threadData->players[playerno] = *player; //Update every thread with the information about the player.

            printf("Player %d has name %s and is of class %d\n", playerno+1, threadData->players[playerno].name, threadData->players[playerno].class+1);

            loop = 0;
        }

    }//end loop

    //PREPARE FOR COMBAT
    // First, we handshake to the client to start the combat
    // SEND
    
    // ++ a la variable compartida
    printf("Player ready for combat \n");
    *(threadData->ready_for_combat)++;


    double chance = threadData->players[playerno].ctH;
    if(!isInterrupted){
        loop = 1;
    }

    sprintf(buffer, "%d", READY);
    sendString(client_fd, buffer); //Ensure client is there and await name and class.

    //

    while(loop && !isInterrupted){

        if(*(threadData->ready_for_combat) == *(threadData->max_players)){
            // POLLIN
        poll_response = poll(poll_list, 1, timeout);

        // Nothing is recieved, we can check if we were interrupted.
        if(poll_response == 0){
            if(isInterrupted){ //If my server was interrupted...
                printf("Thread was interrupted! Closing thread and telling the client...\n");
                serverCode == EXIT;
                sprintf(buffer, "%s %d %d %d", name, serverCode, 0, 0);
                sendString(threadData->players[playerno].connection_fd, buffer); //Ensure client is there and await name and class.

                printf(" Player %d says byebye!\n", playerno+1);
                loop=0;
                break;
            }
                    
        }// End poll
            
            
        //Client sent an action! Lets checkout which one is it
        else if(poll_response == 1){
            // RECIEVE ACTION AND TARGET
             recvString(client_fd, buffer, MAX_BUFSIZE+1);
             sscanf(buffer, "%d %d %d", &serverCode, &action, &target); //Receive name and class selection from client.
             printf("\nRecieved data! %d\n", serverCode);

            //Check what type of action it was.
            if(serverCode == ATTACK){ //ATTACK
                //ATTACK MECHANIC GOES HERE!

                //Player is no longer defending itself.
                threadData->players[playerno].is_defending = 0;

                //1. Lets check if you hit the target
                int r = hit_or_miss(chance);

                if(r == 1){
                    printf("\n %s attacks monster %d for %d damage!\n", name, target, threadData->players[playerno].damage);
                    serverCode = ATTACK;
                }

                else{
                    printf("\n %s has missed his attack!\n", name);
                    serverCode = MISS;
                }

                
                sprintf(buffer, "%s %d %d %d", name, serverCode, target, threadData->players[playerno].damage);

                //SEND TO ALL CLIENTS WHAT HAPPENED
                
                for(int i = 0; i< *(threadData->max_players); i++){
                    sendString(threadData->players[i].connection_fd, buffer); //Ensure client is there and await name and class.
                }
            }

            else if(serverCode == DEFEND){
                // DEFEND MECHANIC GOES HERE!

                //Player is defending itself.
                threadData->players[playerno].is_defending = 1;

                printf("\n %s is defending! Reducing all incoming damage for 50 percent!\n", threadData->players[playerno].name);
                serverCode = DEFEND;
                sprintf(buffer, "%s %d %d %d", threadData->players[playerno].name, serverCode, 0, 0);

                //SEND TO ALL CLIENTS WHAT HAPPENED
                
                for(int i = 0; i< *(threadData->max_players); i++){
                    sendString(threadData->players[i].connection_fd, buffer); //Ensure client is there and await name and class.
                }
                
            }
            
            else if(serverCode == EXIT){
                printf("Client disconnected.");
            }

            else if(serverCode == 0){
                loop=0;
                break;
            }

            else{ // No valid action.
                printf("Error: no valid action recieved %d.",serverCode);
            }

        }
        }//end if

        

    }//end loop

}

// Function to check if player/monster hit or missed the target
int hit_or_miss(double ctH){
    // Returns a double between 0 and 1;
    double r = (double)rand()/(double)RAND_MAX;
    // Compares the result with the given hit chance
    if(r <= ctH){
        return 1; // Hit
    }
    else{
        return 0;// Miss
    }
}

player_t *initEntity(int class, char *name, int hp, int cd, int damage, double ctH)
{
    player_t *entity = malloc(sizeof(player_t));
    entity->class = class;
    entity->name = name;
    entity->hp = hp;
    entity->cooldown = cd;
    entity->damage = damage;
    entity->ctH = ctH;

    return entity;
}

player_t *initMonsters()
{
    player_t *monsters = malloc(5 * sizeof(player_t));
    player_t *slime;
    player_t *murloc;
    player_t *troll;
    player_t *ogre;
    player_t *shiva;

    slime = initEntity(-1, "slime", 30, 2, 5, 0.95);
    murloc = initEntity(-1, "murloc", 50, 4, 15, 0.85);
    troll = initEntity(-1, "troll", 80, 7, 30, 0.80);
    ogre = initEntity(-1, "ogre", 100, 10, 50, 0.55);
    shiva = initEntity(-1, "Final Boss", 300, 5, 40, 0.90);

    return monsters;
}

player_t *initWave(player_t *monsters, int playerNo)
{
    int selecter;
    player_t *wave = malloc(playerNo * sizeof(player_t));
    for (size_t i = 0; i < playerNo; i++)
    {
        selecter = rand() % 5;
        wave[i] = monsters[selecter];
    }

    return wave;
}

// Function to choose a random player for monster to hit
int choose_player(player_t *players, int max_players){
    int selector;

    while(1){
        // Selects a random player
        selector = rand() % max_players;
        if(players[selector].hp > 0){// If the selecte dplayer is alive...
            break;
        }
    }

    //We return him.
    return selector;
    
}

//Before launching this, generate enemies and pass it as arg.
void *monsterThread(void *arg)
{
    thread_t *threadData;
    threadData = (thread_t *)arg;
    char buffer[3*MAX_BUFSIZE];
    int serverCode;

    player_t *monsters = initMonsters();
    player_t *wave= initWave(monsters, *(threadData->max_players));
    int target;
    int damage;
    int sleepy = 7;
    
    threadData->wave = wave;
    
    while(1){
        // Checar varaible compartida, cuando llegue maxplayers, sleep
        if(*(threadData->ready_for_combat) == *(threadData->max_players)){
            printf("MONSTERS ATTACK!\n");
            fflush(stdout);
            //sleep(); flavour text

            for(int i=0; i<*(threadData->max_players);i++){
                int r = hit_or_miss(wave[i].ctH);

                // HIT
                if(r == 1){
                    // Choose a player to hit
                    printf("MONSTER HIT!\n");
                    fflush(stdout);
                    target = choose_player(threadData->players, *(threadData->max_players));

                    //We check if its defending
                    if(threadData->players[target].is_defending){ // Is defending
                        damage = wave[i].damage / 2;
                        threadData->players[target].hp -= damage;
                    }
                    else{ // Its not defending
                        damage = wave[i].damage;
                        threadData->players[target].hp -= damage;
                    }

                    serverCode = ATTACK;
                }

                // MISS
                else{
                    printf("MONSTER MISS!\n");
                    fflush(stdout);
                    serverCode = MISS;
                }

                sprintf(buffer, "%s %d %d %d", wave[i].name, serverCode, target, damage);
                for(int i; i< *(threadData->max_players); i++){
                    sendString(threadData->players[i].connection_fd, buffer); //Ensure client is there and await name and class.
                }

            }//end for

            sleep(sleepy);

        }//end if
    }


}

//TODO: Combat System, Monster Random Generation, Progression, Monster in own thread, Inter-thread communications.



