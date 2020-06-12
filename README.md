# Final Project: Thread Warriors

### Advanced Programming

Members:

[Juan Francisco Gortarez](https://github.com/Starfleet-Command/) A01021926

[Sebastián Gonzalo Vives Faus] (https://github.com/Tlacuachi/) A0102

 

## General Project Description

This is an RPG-inspired quick dungeon crawler where you join a variable number of other players to traverse a dungeon together, as one of either three classes. You will face a random assortment of monsters, from the lowly slime to the dangerous troll, and should you emerge victorious, strike against Shiva, the Lady of Frost. Each class has different strengths and weaknesses, and you must rely on them and your limited moveset to not die!

## Detailed Application Flow

-   You join the game by running the client application with the port you want to connect to. This will lead you to a ‘waiting room’ where the game loops until the (server-configurable) minimum required people have joined.
    

-   After the party is ready, you will be prompted to choose your name and class
    
-   When you do so, you’ll see the initial setup and you’ll experience the limited flavour text this game has to offer
    
-   After this, it’s straight to combat! Monsters will come at you from left and right and you will choose between defending and attacking. You must continue to battle them until you have triumphed or you die.
    
-   If you win, you will regenerate a certain amount of health based on your class.
    
-   If you die, you will be disconnected from the server and will not be able to spectate or participate.
    
-   This process will repeat itself a (server-configurable) number of times. After beating all the waves, you will face the final boss!
    
-   Win or lose, the final boss marks the end of your adventure. You will be disconnected after facing her and must run the program again.
    

 

## Key Elements

The `player_t` structure contains all the data about a player, such as name, class, current hp, and the file descriptor to be used when talking to that client

The `thread_t` structure is all the information that must be passed and shared to each thread, and contains pointers to memory that can be accessed by all threads. Server configurations like the minimum number of players, waves, and the list of all current `player_t`s are found here.

Our thread-based combat works by sending several pointers to all threads, such as  allowing them to see and modify the same locations in memory, which stops the need to constantly communicate the game state to all threads, instead simply relaying them to the client when necessary for clarity.
 

## Functions

### Server

1.**void \*playerThread** (void *arg);

   This is the thread process where the player-client will run and interact with other threads. It’s passed a `thread_t` in the form of a pointer to void. Everything the client does and affects will be stated here. This thread ends and rejoins the main process when the players win, die, or are interrupted either client-side or server-side.

2. **player_t \*initEntity**(int  class, char *name, int  hp, int  cd, int  damage, double  ctH);

    This function creates a `player_t` with the arguments provided and returns a pointer to it. This is used for both monsters and human players.

3. **void \* monsterThread**(void *arg);

   This is another thread process, with the same input requirements as `playerThread`, but this one handles the monsters. It creates their templates, creates the waves of enemies and shares them with the player threads to attack. This thread ends and rejoins the main process when the players win, die, or any thread is interrupted either client-side or server-side.

4. **player_t initMonsters**();

   This function constructs the monster templates using `initEntity` and returns an array containing all the monster templates.

5. **player_t initWave** (player_t *monsters, int  playerNo, int  isFinalBoss);

   This function randomly selects monsters from the `initMonsters` pool of enemies and adds them to a wave array of `player_t`s, which it then returns

6. **int  hit_or_miss** (double  ctH);

   This is a small helper function that uses RNG and a character’s (player or enemy) chance to hit to determine if that character will hit the intended target or not. It returns 1 on hit or 0 on a miss.

7. **int  choose_player** (player_t *players, int  max_players);

   This function randomly determines a target from among the available ones and returns the position in the `player_t` array that it will target. In case of no target being available (all monsters or players have died) it returns -1. This function is also used to correct mistypes from the client: if they target an already-dead target or one that does not exist, this function will correct their choice and choose for them instead.

8. **int  combatEnded** (player_t *attackers, player_t *defenders, int  numPlayers);

   This function detects if all entities inside either attackers or defenders have died (hp<0). It returns 1 if all defenders are dead, -1 if all attackers are dead, and 0 if none are dead.

9. **void  addHealth** (player_t *players, int  max_players);
   This function restores health to all players in `*players` after a wave has been cleared.

10. **void  printToClients**(player_t *players, int  max_players, char *buffer);
   This function sends one event, stored in `buffer`, to all clients currently connected who are still alive. This functionality is different from printing multiple events to multiple clients.

 

### Client

1. **void characterCreation**(char * buffer, char * name, int option, int server_fd);

   This function asks the client for his/her name, as well as the class they wish to play as. Once the player has given a name and class, it will send it to the server (its thread).

2. **void showStory**();

   This function prints the introduction of the story to the client, using sleep() to give the player time to read the text and time for the monster thread to check and prepare for combat.

3. **int printClient**(char *pch, char * name, int serverCode, char *target_name, int target, int damage, int health);

   After something happened in the server (if the player attacked something, if he/she defended, a monster attacked him/her, etc), the server sends the serverCode to the client (as well as what happened), where in this function the client reads it and prints the specific action.

## Dependencies
* System capable of recognizing and running the ` pthread, unistd, netdb and sys/poll`   c libraries, such as the WSL or a Unix System.
* gcc installed
* The ` sockets.h` , ` sockets.c` , and ` project_codes.h` files in the same directory as the main files.
 

## Usage

**Compiling with make**

`make clean all`

**Compiling with gcc**

`gcc final_project.c -o finProj -g -lpthread -Wall -pedantic && gcc final_client.c -o finClient -g -lpthread -Wall -pedantic`

 

**Running the server**

`./finProj {port number} {minimum players for party} {number of waves}`

 

**Running the client**

`./finClient {server ip address} {server port number}`


