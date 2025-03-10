/*

    Specific functions for the Ticket to Ride game.

    Require codingGameServer.c, codingGameServer.h, lib/json.h to works with.

    Authors: Valentin Le Lièvre
    Licence: GPL

    Copyright 2025 Valentin Le Lièvre

*/

/*

    1. How to use:
        To connect to the server and play a game you have to call (in order):
            - int connectToCGS(char* address, unsigned int port)
            - int sendName(char* name)
            - int sendGameSettings(GameSettings gameSettings, GameData* gameData)

        You will then be connected to a game and will be able to play by calling:
            - int getMove(MoveData *moveData)
            - int sendMove(unsigned int move, int* moveType)

    2. Constants:
        To communicate actions to the server you can use CONSTANTS variables, defined
        in enum types below, instead of hex code values.

        For exemple: use the TRAINING constant instead of code 0x1. It will greatly improve code readability.

    3. Defaults values for struct:
        You can instantiate struct with pre-set default values by using:
            - GameSettings gameSettings = GameSettingsDefaults;
            - GameData gameData = GameDataDefaults;
            - MoveData moveData = MoveDataDefaults;

        This will reduce potential errors and unexpected behaviours.

    4. Every function will return an int indicating the success / failure of the function.
        Possible error codes are:
            - 0x10: Param errors
            - 0x20: server / network errors
            - 0x30: other error
            - 0x40: all good

        Be sure to check return values of functions to handle errors. You may want to use the constants for better readability.

        Exemple: if(result == SERVER_ERROR) { ... }

    5. Memory management:
        Some functions are using malloc calls to allocate memory space. You will need
        to free those spaces.

        Variables that need to be freed are:
            - gameName from GameData struct
            - board from GameData struct
            - opponentMessage from MoveData struct

        NOTE: you are likely to create multiple instance of MoveData, don't forget
        to free opponentMessage, or you will likely encounter Segmentation fault error.

*/

#ifndef TICKET_TO_RIDE_H
#define TICKET_TO_RIDE_H

#include <stdbool.h>

/*
    Structs
*/

typedef enum {
    PARAM_ERROR = 0x10,
    SERVER_ERROR = 0x20,
    OTHER_ERROR = 0x30,
    MEMORY_ALLOCATION_ERROR = 0x40,
    ALL_GOOD = 0x50
} ResultCode;

typedef enum {
    NO_DEBUG = 0x0,
    MESSAGE,
    DEBUG,
    INTERN_DEBUG
} DebugLevel;

typedef enum {
    NORMAL_MOVE = 0x1,
    LOOSING_MOVE = 0x2,
    WINNING_MOVE = 0x3,
    ILLEGAL_MOVE = 0x4,

    StateMax // Keep as last element
} MoveState;



typedef enum {
    TRAINING = 0x1, // Play against a bot
    MATCH = 0x2, // Play against a player
    TOURNAMENT = 0x3, // Enter a tournament

    GamesTypesMax // Keep as last element, is used for params checking
} GamesType;

typedef enum {
    RANDOM_PLAYER = 0x1,
    NICE_BOT,

    BotsNamesMax // Keep as last element
} BotsNames;



typedef enum {
    CLAIM_ROUTE = 0x1,  // Claim a route between two cities

    DRAW_BLIND_CARD, // Draw a card from the deck
    DRAW_CARD, // Draw a card from the visible cards

    DRAW_OBJECTIVES, // Draw 3 objectives
    CHOOSE_OBJECTIVES // Choose 1 to 3 objectives
} Action;

typedef enum {
    NONE = 0,
    PURPLE = 1,
	WHITE = 2,
	BLUE = 3,
	YELLOW = 4,
	ORANGE = 5,
	BLACK = 6,
	RED = 7,
	GREEN = 8,
	LOCOMOTIVE = 9
} CardColor;

typedef struct {
    unsigned int from;
    unsigned int to;
    unsigned int score;
} Objective;

typedef struct {
    unsigned int from;
    unsigned int to;
    CardColor color;
    unsigned int nbLocomotives;
} ClaimRouteMove;


typedef struct {
    Action action; // One of Actions values

    union {
        ClaimRouteMove claimRoute;      // the route we claim
        CardColor drawCard;             // the card we draw
        bool chooseObjectives[3];       // the objectives we choose
    };
} MoveData;

typedef struct MoveResult_ {
    MoveState state; // One of MoveState values

    union {
        CardColor card;
        Objective objectives[3];
    };

    char* opponentMessage; // String containing a message send by the opponent
    char* message; // String containing a message send by the server
} MoveResult;

typedef struct {
        CardColor card[5]; // Visible cards
} BoardState;

typedef struct {
    GamesType gameType; // One of GamesTypes values
    BotsNames botId; // One of BotsName values (only if you play in training)

    unsigned int timeout; // Timeout in seconds, max value 60, default 15 (used only in training mode, tournament mode is set to 15)
    unsigned char starter; // Define who starts, 1 -> you or 2 -> opponent, set to 0 for random, default 0 (used only in training mode)
    unsigned int seed; // Seed for the board generation, useful when debugging to play the same game again, max value 9999, default to 0 for random (used only in training mode)

    unsigned char reconnect; // Set 1 if you want to reconnect to a game you already started, default 0
} GameSettings;

typedef struct {
    char* gameName; // String containing the game name
    int gameSeed; // Contain the seed used for the game board generation (if you didn't provide one)
    int starter; // Defines who start, 1 you and 2 opponent

    int nbCities; // Total number of cities in the map
    int nbTracks;   // total number of tracks
    int* trackData; // Track data, an array containing unformatted data about the tracks
    CardColor cards[4];
} GameData;




/*

    Default values for struct

    You can use those variables to initialize struct with default values

*/

extern const GameSettings GameSettingsDefaults;
extern const GameData GameDataDefaults;

/*

    Exposed functions

*/

// This is the first function you should call, it will connect you to the server.
// You need to provide the server address and the port to connect to.
// This is a blocking function, it will wait until the connection is established, it may take some time.
ResultCode connectToCGS(const char *address, unsigned int port);

// After connecting to the server you need to send your name to the server. It will be used to uniquely identify you.
// You need to provide your name as a string. It should be less than 90 characters long.
ResultCode sendName(const char *name);

// After sending your name you need to send game settings to the server to start a game.
// You need to provide a GameSettings struct and a GameData struct to store the game data returned by the server.
// You can use the GameSettingsDefaults and GameDataDefaults variables to initialize the struct with default values.
// To fill the GameSettings struct you may want to use predefined constants available in codingGameServer.h.
ResultCode sendGameSettings(GameSettings gameSettings, GameData* gameData);

// During a game this function is used to know what your opponent did during his turn.
// You need to provide an empty MoveData struct and an empty MoveResult struct to store the move data returned by the server.
// MoveData struct store the move your opponent did and MoveResult struct store the result of the move.
ResultCode getMove(MoveData* moveData, MoveResult* moveResult);

// During a game this function is used to send your move to the server.
// You need to provide a MoveData struct containing your move and an empty MoveResult struct to store the result of the move returned by the server.
ResultCode sendMove(const MoveData *moveData, MoveResult* moveResult);

// This function is used to get the current state of the board during a game.
ResultCode getBoardState(BoardState* boardState);

// This function is used to send a message to your opponent during a game.
// You need to provide the message as a string. It should be less than 256 characters long.
ResultCode sendMessage(const char *message);

// This function is used to display the game board during a game.
// It will print the colored board in the console.
ResultCode printBoard();

// Prints the city name
ResultCode printCity(unsigned int cityId);

// This function is used to quit the currently running game.
ResultCode quitGame();


#endif