/*
    Final project program
    An AFK clicker-style game with linear progression and classes. 

    Juan Francisco Gortarez Ricardez A01021926
    Sebastian Gonzalo Vives Faus A01025211
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

#define MAX_BUFSIZE 1024
#define MAX_QUEUE 7
#define MAX_PLAYERS 5
#define FLAVOUR_SLEEP 10
#define ADDHEALTH 40

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

//Structure for mutexes to keep data consistent
typedef struct locks_struct {
    // Mutex array for the operations on the threads
    pthread_mutex_t * monster_mutex;
} locks_t;

typedef struct thread_info //Information sent to each thread about other processes
{
    player_t *players;
    int connection_fd;
    player_t *wave;
    int playerno;
    int *ready_for_combat;
    int *max_players;
    int *max_waves;
    // A pointer to a locks structure
    locks_t * data_locks;
} thread_t;

int isInterrupted = 0;

//--------------FUNCTION DECLARATIONS--------------------------

void usage(char *program);
void setupHandlers();
sigset_t setupMask();
void sendString(int connection_fd, char *buffer);
void waitForConnections(int server_fd, int max_players, int max_waves);
void onInterrupt(int signal);
void *playerThread(void *arg);
player_t *initEntity(int class, char *name, int hp, int cd, int damage, double ctH);
void *monsterThread(void *arg);
player_t *initWave(player_t *monsters, int playerNo, int isFinalBoss, player_t * wave);
player_t *initMonsters();
int hit_or_miss(double ctH);
int choose_player(player_t *players, int max_players);
int combatEnded(player_t *attackers, player_t *defenders, int numPlayers);
void addHealth(player_t *players, int max_players);
void printToClients(player_t *players, int max_players, char *buffer);
void prepare_message(char * superbuffer, int max_players, player_t *wave, int serverCode);
//--------------------------------------------------------------

int main(int argc, char *argv[])
{
    int max_players;
    int max_waves;
    if (argc != 4)
    {
        usage(argv[0]);
    }

    int server_fd;
    if (atoi(argv[2]) > 0 || atoi(argv[3]) > 0)
    {
        max_players = atoi(argv[2]);
        max_waves = atoi(argv[3]);
    }
    else
    {
        perror("Values for max_players or waves too low");
        exit(1);
    }

    srand(time(NULL));

    //Initialize the thread_t struct

    printf("\n=== WELCOME TO THE THREAD WARRIORS ===\n");

    // Check the correct arguments

    // Configure the handler to catch SIGINT
    setupHandlers();
    setupMask();

    // Initialize the data structures
    //Data locks
    // Show the IPs assigned to this computer
    printLocalIPs();
    // Start the server
    server_fd = initServer(argv[1], MAX_QUEUE);
    // Listen for connections from the clients
    waitForConnections(server_fd, max_players, max_waves);
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
    printf("\t <number of players> ");
    printf("\t <number of waves>\n");
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

void waitForConnections(int server_fd, int max_players, int max_waves)
{
    thread_t *threadinfo = malloc(sizeof(thread_t)); //Shared memory space for all threads to use
    int *ready_for_combat = malloc(sizeof(int));
    int *t_max_players = malloc(sizeof(int));
    int *t_max_waves = malloc(sizeof(int));
    locks_t data_locks;
    //MUTEX

    //Assign memory
    data_locks.monster_mutex = malloc((max_players)*sizeof(pthread_mutex_t));

    for (int i=0; i<(max_players); i++)
    {
        pthread_mutex_init(&data_locks.monster_mutex[i], NULL);
    }


    *(t_max_waves) = max_waves;
    *(t_max_players) = max_players;
    *(ready_for_combat) = 0;
    struct sockaddr_in client_address;
    socklen_t client_address_size;
    char client_presentation[INET_ADDRSTRLEN];
    int client_fd;
    int *client_fds = malloc(sizeof(int) * MAX_PLAYERS);
    // Pollin
    int timeout = 500;     // Time in milliseconds (0.5 seconds)
    int poll_response = 0; //Poll return value

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

    while (loop)
    {

        // POLLIN
        poll_response = poll(test_fds, 1, timeout);

        // Nothing is recieved, we can check if interrupted.
        if (poll_response == 0)
        {

            if (isInterrupted)
            { //If my server was interrupted...
                printf("Server was interrupted! Closing server...\n");
                break;
            }

            //There is 3 players, we are not expecting nothing else, we can create the warrios...
            else if (playerNo == max_players && createWarrior)
            {
                //Create player threads
                printf("All players ready! Creating warrios...\n");
                player_t *players = malloc(sizeof(player_t) * playerNo); //Allocate memory for player array
                threadinfo->players = players;
                threadinfo->ready_for_combat = ready_for_combat;
                threadinfo->max_players = t_max_players;
                threadinfo->max_waves = t_max_waves; //Pass the max waves to both monsters and players
                threadinfo->data_locks = &data_locks;

                for (int i = 0; i < max_players; i++)
                {
                    //Put the appropriate info to send to thread
                    threadinfo->connection_fd = client_fds[i];
                    threadinfo->playerno = i;

                    // CREATE A THREAD
                    int status = pthread_create(&new_tid, NULL, &playerThread, threadinfo);
                    printf("Created warrior %d with status %d\n", i, status);
                    // Check if the thread was created correctly
                    if (status == -1)
                    {
                        // The thread was NOT created correctly.
                        perror("ERROR: pthread_create");
                        // Close the client connection
                        close(client_fd);
                    }
                } // end for

                //Create monster thread
                int status = pthread_create(&new_tid, NULL, &monsterThread, threadinfo);
                if (status == -1)
                {
                    // The thread was NOT created correctly.
                    perror("ERROR: pthread_create");
                    // Close the client connection
                    close(client_fd);
                }

                playerNo = 0;
            }
        } // end if

        // There is something coming in, which most probably is a new connection from a client, recieve it.
        else if (poll_response == 1)
        {

            //There is less than max_players players
            if (playerNo < max_players && createWarrior)
            {
                // Accept new connection
                client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_address_size);
                // Check if connection is valid
                if (client_fd == -1)
                {
                    perror("ERROR: accept");
                }
                else
                {
                    client_fds[playerNo] = client_fd; //Add to client fd array.
                    playerNo++;
                    // Get the data from the client
                    inet_ntop(client_address.sin_family, &client_address.sin_addr, client_presentation, sizeof client_presentation);
                    printf("Received incoming connection from %s on port %d\n Remaining players... %d\n", client_presentation, client_address.sin_port, max_players - playerNo);
                }
            } // end if

        } // end else if

        //Else, I got an error
        else
        {
            perror("ERROR: poll");
        }
    }

    free(threadinfo);
    free(t_max_players);
    free(ready_for_combat);
    free(t_max_waves);
    free(client_fds);
}

void *playerThread(void *arg)
{
    thread_t *threadData = NULL;
    int client_fd;
    player_t *player = NULL;
    int class = 1;
    int action = 0;
    int target = 0;
    int serverCode;
    threadData = (thread_t *)arg;
    client_fd = threadData->connection_fd;
    int playerno = threadData->playerno;
    char buffer[3 * MAX_BUFSIZE];
    char name[MAX_BUFSIZE];
    int altTarget;
    int wavesRemaining = 0;
    int playersInGame;
    // Create a structure array to hold the file descriptors to poll
    struct pollfd poll_list[1];
    // Fill in the structure
    poll_list[0].fd = client_fd;
    poll_list[0].events = POLLIN; // Check for incomming data
                                  // Pollin
    int timeout = 500;            // Time in milliseconds (0.5 seconds)
    int poll_response = 0;        //Poll return value

    int loop = 1;

    // HANDSHAKE WITH CLIENT
    // SEND
    sprintf(buffer, "%d", SYN);
    sendString(client_fd, buffer); //Ensure client is there and await name and class.

    while (loop && !isInterrupted)
    {
        // POLLIN
        poll_response = poll(poll_list, 1, timeout);

        // Nothing is recieved, we can check if interrupted.
        if (poll_response == 0)
        {
            if (isInterrupted)
            { //If my server was interrupted...
                printf("Thread was interrupted! Closing thread...\n");
                break;
            }
        }

        // There is something coming in, which most probably is the information from the client.
        else if (poll_response == 1)
        {
            // RECIEVE CLASS AND NAME FROM CLIENT
            recvData(client_fd, buffer, MAX_BUFSIZE + 1);
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

            printf("Player %d has name %s and is of class %d\n", playerno + 1, threadData->players[playerno].name, threadData->players[playerno].class + 1);

            loop = 0;
        }

    } //end loop

    //PREPARE FOR COMBAT
    // First, we handshake to the client to start the combat
    // SEND

    *(threadData->ready_for_combat) += 1;
    printf("Player ready for combat %d \n", *(threadData->ready_for_combat));
    wavesRemaining = *(threadData->max_waves) + 2;

    double chance = threadData->players[playerno].ctH;
    if (!isInterrupted)
    {
        loop = 1;
    }

    playersInGame = *(threadData->max_players);

    sprintf(buffer, "%d %d", READY, playersInGame);
    sendString(client_fd, buffer); //Tell client game is starting and that it will send wave info.

    sleep(FLAVOUR_SLEEP); //SLEEP FOR CLIENT FLAVOUR TEXT GOES HERE

    while (wavesRemaining >= 0 && !isInterrupted)
    {

        if (*(threadData->ready_for_combat) == playersInGame)
        {
            // POLLIN
            poll_response = poll(poll_list, 1, timeout);

            // Nothing is recieved, we can check if we were interrupted.
            if (poll_response == 0)
            {
                if (isInterrupted)
                { //If my server was interrupted...
                    printf("Thread was interrupted! Closing thread and telling the client...\n");
                    //serverCode == EXIT;
                    sprintf(buffer, "%s %d %s %d %d %d", name, 7, name, 0, 0, 0);
                    sendString(threadData->players[playerno].connection_fd, buffer); //Ensure client is there and await name and class.

                    printf(" Player %d interrupted! Exiting...\n", playerno + 1);
                    break;
                }

                if (threadData->players[playerno].hp <= 0)
                {
                    printf("Player %s has died! Exiting...", threadData->players[playerno].name);
                    //serverCode == 7;
                    sprintf(buffer, "%s %d %s %d %d %d", name, 7, name, 0, 0, 0);
                    sendString(threadData->players[playerno].connection_fd, buffer); //Ensure client is there and await name and class.

                    //*(threadData->max_players)--;
                    threadData->players[playerno].hp = 0;
                    break;
                }

            } // End poll

            //Client sent an action! Lets checkout which one is it
            else if (poll_response > 0)
            {
                // RECIEVE ACTION AND TARGET
                recvData(client_fd, buffer, MAX_BUFSIZE + 1);
                sscanf(buffer, "%d %d %d", &serverCode, &action, &target); //Receive name and class selection from client.

                //printf("\nRecieved data! %d\n", serverCode);

                //Check what type of action it was.
                if (serverCode == ATTACK)
                { //ATTACK

                    //Player is no longer defending itself.
                    threadData->players[playerno].is_defending = 0;

                    //1. Lets check if you hit the target
                    int r = hit_or_miss(chance);

                    if (r == 1)
                    {
                        if (threadData->wave[target].hp > 0) //Target is valid and not dead
                        {
                            //MUTEX LOCK
                            pthread_mutex_lock(&threadData->data_locks->monster_mutex[target]);

                            threadData->wave[target].hp -= threadData->players[playerno].damage;

                            //MUTEX UNLOCK
                            pthread_mutex_unlock(&threadData->data_locks->monster_mutex[target]);

                            printf("\n %s attacks monster %d for %d damage! It has %d hp left. \n", name, target, threadData->players[playerno].damage, threadData->wave[target].hp);
                            sprintf(buffer, "%s %d %s %d %d %d", name, serverCode, threadData->wave[target].name, 0, threadData->players[playerno].damage, threadData->wave[target].hp);
                        }

                        else //If target is dead randomly select a non-dead target
                        {
                            altTarget = choose_player(threadData->wave, playersInGame);
                            if (altTarget != -1)
                            {
                                //MUTEX LOCK
                                pthread_mutex_lock(&threadData->data_locks->monster_mutex[altTarget]);

                                threadData->wave[altTarget].hp -= threadData->players[playerno].damage;

                                //MUTEX UNLOCK
                                pthread_mutex_unlock(&threadData->data_locks->monster_mutex[altTarget]);

                                printf("\n %s attacks monster %d for %d damage! It has %d hp left. \n", name, altTarget, threadData->players[playerno].damage, threadData->wave[altTarget].hp);
                                sprintf(buffer, "%s %d %s %d %d %d", name, serverCode, threadData->wave[altTarget].name, 0, threadData->players[playerno].damage, threadData->wave[altTarget].hp);
                            }
                            else if (altTarget == -1 && wavesRemaining > 0) //This means the wave is over.
                            {
                                printf("No more targets available. Wait until thread reappears");
                                sprintf(buffer, "%s %d %s %d %d %d", name, serverCode, threadData->wave[0].name, 0, 0, 0); //As a way to show that no damage was dealt.
                            }
                            else //This means all waves are over. This is to stop threads from continuing.
                                break;
                        }

                        serverCode = ATTACK;
                    }

                    else
                    {
                        printf("\n %s has missed his attack!\n", name);
                        serverCode = MISS;
                        sprintf(buffer, "%s %d %s %d %d %d", name, serverCode, threadData->wave[target].name, 0, threadData->players[playerno].damage, threadData->wave[target].hp);
                    }

                    //SEND TO ALL CLIENTS WHAT HAPPENED

                    printToClients(threadData->players, playersInGame, buffer);
                }

                else if (serverCode == DEFEND)
                {
                    // DEFEND MECHANIC

                    //Player is defending itself.
                    threadData->players[playerno].is_defending = 1;

                    printf("\n %s is defending! Reducing all incoming damage for 50 percent!\n", threadData->players[playerno].name);
                    serverCode = DEFEND;
                    sprintf(buffer, "%s %d %s %d %d %d", threadData->players[playerno].name, serverCode, name, 0, 0, threadData->players[playerno].hp);

                    //SEND TO ALL CLIENTS WHAT HAPPENED
                    printToClients(threadData->players, playersInGame, buffer);
                }

                else if (serverCode == EXIT)
                {
                    printf("Client disconnected.");
                    threadData->players[playerno].hp = 0; //Stop enemies from targeting the player who left
                    break;
                }

                else
                { // No valid action.
                    printf("Error: no valid action recieved %d.", serverCode);
                }

                if (combatEnded(threadData->players, threadData->wave, *(threadData->max_players)))
                {
                    sleep(3); //Sleep to allow time for wave to regen. TEMPORARY
                    wavesRemaining--;
                    printf("waves remaining: %d \n", wavesRemaining);
                    //SLEEP FOR FURTHER FLAVOUR TEXT IF REQUIRED GOES HERE
                }
            }

            else
            {
                if (isInterrupted)
                { //If my server was interrupted...
                    printf("Thread was interrupted! Closing thread and telling the client...\n");
                    serverCode = EXIT;
                    sprintf(buffer, "%s %d %s %d %d %d", name, serverCode, name, 0, 0, 0);
                    sendString(threadData->players[playerno].connection_fd, buffer); //Ensure client is there and await name and class.

                    printf(" Player %d's thread was interrupted!\n", playerno + 1);
                    break;
                }
            }
        } //end if

    } //end loop
    printf("\nThread no %d exiting\n", playerno + 1);
    free(player);
    pthread_exit(NULL);
}

// Function to check if player/monster hit or missed the target
int hit_or_miss(double ctH)
{
    // Returns a double between 0 and 1;
    double r = (double)rand() / (double)RAND_MAX;
    // Compares the result with the given hit chance
    return r <= ctH;
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

player_t *initMonsters() //Creation of the array where the monster templates are held for later cloning.
{

    player_t *monsters = malloc(5 * sizeof(player_t));
    player_t *slime;
    player_t *murloc;
    player_t *troll;
    player_t *ogre;
    player_t *shiva;

    slime = initEntity(-1, "Slime", 30, 2, 5, 0.95);
    murloc = initEntity(-1, "Murloc", 50, 4, 15, 0.85);
    troll = initEntity(-1, "Troll", 80, 7, 30, 0.80);
    ogre = initEntity(-1, "Ogre", 100, 10, 50, 0.55);
    shiva = initEntity(-1, "Shiva", 300, 4, 80, 0.80);

    monsters[0] = *slime;
    monsters[1] = *murloc;
    monsters[2] = *troll;
    monsters[3] = *ogre;
    monsters[4] = *shiva;

    return monsters;
}

player_t *initWave(player_t *monsters, int playerNo, int isFinalBoss, player_t * wave) //Create a wave of enemies or a wave of only the final boss
{
    int selecter;

    if (isFinalBoss)
    {
        wave[0] = monsters[4];
    }

    else
    {

        for (int i = 0; i < playerNo; i++) //RNG to clone monsters from template and add them to the wave
        {
            selecter = rand() % 4;
            wave[i] = monsters[selecter];
            printf("Monster with name  %s added to wave \n", wave[i].name);
            fflush(stdout);
        }
    }

    return wave;
}

// Function to choose a random player for monster to hit
int choose_player(player_t *players, int max_players)
{
    int selector;
    int dead_players = 0;
    ;

    for (int i = 0; i < max_players; i++)
    {
        if (players[i].hp <= 0)
        {
            dead_players++;
        }
    }

    if (dead_players >= max_players) //If all players are dead no targets can be found.
    {
        printf("All players dead! No targets available\n");
        return -1;
    }

    while (1)
    {
        // Selects a random player

        selector = rand() % max_players;
        if (players[selector].hp > 0)
        { // If the selected player is alive choose it. Otherwise look for another
            break;
        }
    }

    //We return them.
    return selector;
}

//Function to test if all members of a team have died or if combat is still running
int combatEnded(player_t *attackers, player_t *defenders, int numPlayers)
{
    int attackersDead = 0;
    int defendersDead = 0;
    for (size_t i = 0; i < numPlayers; i++)
    {
        if (attackers[i].hp <= 0)
        {
            printf("attacker named %s is dead \n", attackers[i].name);
            attackersDead++;
        }
        if (defenders[i].hp <= 0)
        {
            printf("defender named %s is dead \n", defenders[i].name);
            defendersDead++;
        }
    }

    if (defendersDead == numPlayers && attackersDead != numPlayers) //If attackers have won
    {
        return 1;
    }

    if (attackersDead == numPlayers && defendersDead != numPlayers) //If attackers have lost
    {
        return -1;
    }
    else
        return 0;
}

void addHealth(player_t *players, int max_players)
{
    for (int s = 0; s < max_players; s++)
    {
        if (players[s].hp > 0)
        {
            players[s].hp += ADDHEALTH;
        }
    }
}

void printToClients(player_t *players, int max_players, char *buffer)
{
    for (int i = 0; i < max_players; i++)
    {
        if (players[i].hp > 0)
        {
            sendString(players[i].connection_fd, buffer);
        }
    }
}

void prepare_message(char * superbuffer, int max_players, player_t *wave, int serverCode){
    char buffer[MAX_BUFSIZE];
    superbuffer[0] = '\0'; // Clear superbuffer

    for(int j = 0; j < max_players; j ++){ // For to print each monster
            sprintf(buffer, "%s %d %s %d %d %d:", wave[j].name, serverCode, " ", 0, j, wave[j].hp);
            strcat(superbuffer, buffer);
    }
}

//Before launching this, generate enemies and pass it as arg.
void *monsterThread(void *arg)
{
    thread_t *threadData = NULL;
    threadData = (thread_t *)arg;
    char buffer[MAX_BUFSIZE];
    char superbuffer[*(threadData->max_players) * (MAX_BUFSIZE + 1)];
    int serverCode;
    int wavesRemaining = *(threadData->max_waves);
    int monsterCount = *(threadData->max_players);

    player_t *monsters = initMonsters();
    player_t *wave = malloc(monsterCount * sizeof(player_t));
    wave = initWave(monsters, monsterCount, 0, wave); //Create initial wave.
    int target;
    int attackDamage;
    int sleepy = 0;
    int isFinalBoss = 0;

    threadData->wave = wave;


    while (wavesRemaining >= 0 && !isInterrupted)
    {
        // Only begin combat when all threads signal they are also ready
        if (*(threadData->ready_for_combat) >= monsterCount)
        {
            if (wavesRemaining == *(threadData->max_waves))
            {
                sleep(FLAVOUR_SLEEP); //Only sleep when it's the first wave to allow for flavour text.
                //Send clients WAVE information:
                serverCode = WAVEINFO;
                prepare_message(superbuffer, *(threadData->max_players), wave, serverCode);
                printToClients(threadData->players, *(threadData->max_players), superbuffer);
            }
            for (size_t i = 0; i < monsterCount; i++) //Dynamic cooldown calculation: average of the cd of each monster in wave.
            {
                sleepy += threadData->wave[i].cooldown;
            }
            sleepy = sleepy / monsterCount;

            printf("This wave's cooldown is %d \n", sleepy);

            sleep(3); //Arbitrary time to stop monsters from attacking immediately
            while (!combatEnded(threadData->wave, threadData->players, *(threadData->max_players)) && !isInterrupted)
            {

                for (int i = 0; i < monsterCount; i++)
                {
                    if (threadData->wave[i].hp > 0)
                    {
                        int r = hit_or_miss(threadData->wave[i].ctH);

                        // HIT
                        if (r == 1)
                        {
                            // Choose a player to hit
                            target = choose_player(threadData->players, *(threadData->max_players));

                            if (target == -1)
                            {
                                printf("while All players dead!\n");
                                continue;
                            }

                            //We check if its defending
                            if (threadData->players[target].is_defending)
                            { // Is defending
                                attackDamage = threadData->wave[i].damage / 2;
                                threadData->players[target].hp -= attackDamage;
                            }
                            else
                            { // Its not defending
                                attackDamage = threadData->wave[i].damage;
                                threadData->players[target].hp -= attackDamage;
                            }

                            serverCode = ATTACK;
                        }

                        // MISS
                        else
                        {

                            serverCode = MISS;
                        }

                        sprintf(buffer, "%s %d %s %d %d %d", threadData->wave[i].name, serverCode, threadData->players[target].name, 0, attackDamage, threadData->players[target].hp);
                        printf("Monster named %s attacked %d for %d damage\n", threadData->wave[i].name, target, attackDamage);
                        for (int s = 0; s < *(threadData->max_players); s++)
                        {
                            if (threadData->players[s].hp > 0)
                            {
                                sendData(threadData->players[s].connection_fd, buffer, strlen(buffer) + 1); //Send combat information by monsters to client.
                            }
                        }
                    }

                } //end for

                sleep(sleepy); //Cooldowns
            }
            sleepy = 0; //Reset cooldown calculator
            //After combat, create a new wave and wait again for flavour text

            //If monsters WIN
            if (combatEnded(threadData->wave, threadData->players, *(threadData->max_players)) == 1)
            {
                printf("\nMonsters have won!\n");
                break;
            }

            //Final boss enter
            if (wavesRemaining == 0 && isFinalBoss == 0)
            {
                wavesRemaining++; //Do this so loop doesn't stop.

                monsterCount = 1; //Only final boss present for last fight
                isFinalBoss = 1;
                wave = initWave(monsters, monsterCount, isFinalBoss, wave); //Clear everything in wave and set only final boss
                printf("Previous wave is dead. Creating FINAL BOSS WAVE \n");
                addHealth(threadData->players, *(threadData->max_players));
                serverCode = NEWWAVE;
                threadData->wave = wave;
                for (int s = 0; s < *(threadData->max_players); s++)
                {
                    if (threadData->players[s].hp > 0)
                    {
                        serverCode = NEWWAVE;
                        sprintf(buffer, "%s %d %s %d %d %d", threadData->players[s].name, serverCode, threadData->players[s].name, 0, 0, threadData->players[s].hp);
                        sendData(threadData->players[s].connection_fd, buffer, strlen(buffer) + 1); //Send combat information by monsters to client
                    }
                }

                serverCode = WAVEINFO;
                prepare_message(superbuffer, *(threadData->max_players), wave, serverCode);
                printToClients(threadData->players, *(threadData->max_players), superbuffer);
                
            }

            else if (wavesRemaining == 1 && isFinalBoss == 1)
            {
                printf("Final boss dead \n");
                wavesRemaining = -1;
                serverCode = VICTORY;
                sprintf(buffer, "%s %d %s %d %d %d", threadData->wave[0].name, serverCode, threadData->wave[0].name, 0, 0, 0);
                printToClients(threadData->players, *(threadData->max_players), buffer);
                break;
            }
            else if (wavesRemaining > 0)
            {
                wavesRemaining--;
                wave = initWave(monsters, *(threadData->max_players), 0, wave);
                threadData->wave = wave;
                printf("Previous wave is dead. Creating new one \n");
                addHealth(threadData->players, *(threadData->max_players));
                
                for (int s = 0; s < *(threadData->max_players); s++)
                {
                    serverCode = NEWWAVE;
                    if (threadData->players[s].hp > 0)
                    {
                        sprintf(buffer, "%s %d %s %d %d %d", threadData->players[s].name, serverCode, threadData->players[s].name, 0, 0, threadData->players[s].hp);
                        sendData(threadData->players[s].connection_fd, buffer, strlen(buffer) + 1); //Send combat information by monsters to client.
                        serverCode = WAVEINFO;
                    }
                }

                serverCode = WAVEINFO;
                prepare_message(superbuffer, *(threadData->max_players), wave, serverCode);
                printToClients(threadData->players, *(threadData->max_players), superbuffer);
                
            }

        } //end if
    }
    free(monsters);
    free(wave);
    printf("Monster thread exiting\n");
    pthread_exit(NULL);
}

//TODO: Mutexes, Fix Interrupts, Signal to client that fight has stopped, apply cooldowns to user attacks, notify them of their deaths.