/*
+----------------------------------------------------+
|                                                    |
|        #######                                     |
|           #    #  ####  #    # ###### #####        |
|           #    # #    # #   #  #        #          |
|           #    # #      ####   #####    #          |
|           #    # #      #  #   #        #          |
|           #    # #    # #   #  #        #          |
|           #    #  ####  #    # ######   #          |
|                                                    |
|                      ######                        |
|      #####  ####     #     # # #####  ######       |
|        #   #    #    #     # # #    # #            |
|        #   #    #    ######  # #    # #####        |
|        #   #    #    #   #   # #    # #            |
|        #   #    #    #    #  # #    # #            |
|        #    ####     #     # # #####  ######       |
|                                                    |
|                                                    |
+----------------------------------------------------+

Authors: T. Hilaire, V. Le Lièvre
Licence: GPL

File: TicketToRide.h
	Client API for the TicketToRide game with CGS

Copyright 2025 T. Hilaire, V. Le Lièvre
*/


/*

    1. How to use:
        To connect to the server and play a game you have to call (in order):
            - ResultCode connectToCGS(const char *address, unsigned int port)
            - ResultCode sendName(const char *name)
            - ResultCode sendGameSettings(GameSettings gameSettings, GameData* gameData)

        You will then be connected to a game and will be able to play by calling:
            - ResultCode getMove(MoveData* moveData, MoveResult* moveResult)
            - ResultCode sendMove(const MoveData *moveData, MoveResult* moveResult)

    2. Constants:
        To communicate actions to the server you can use CONSTANTS variables, defined
        in enum types below, instead of hex code values.

        For exemple: use the TRAINING constant instead of code 0x1. It will greatly improve code readability.

    3. Defaults values for struct:
        You can instantiate struct with pre-set default values by using:
            - GameSettings gameSettings = GameSettingsDefaults;
            - GameData gameData = GameDataDefaults;

        This will reduce potential errors and unexpected behaviours.

    4. Every function will return an int (ResultCode) indicating the success / failure of the function.
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

        Variables that need to be freed are detailed in the comment of each function

        NOTE: you are likely to create multiple instance of MoveData, don't forget
        to free opponentMessage, or you will likely encounter Segmentation fault error.

*/


#ifndef __TICKET_TO_RIDE_H__
#define __TICKET_TO_RIDE_H__
#include "clientAPI.h"
#include <stdbool.h>



/* The 5 different type of moves
 * The `DRAW_OBJECTIVES` move must be followed by a `CHOOSE_OBJECTIVES` move
 */
typedef enum {
    CLAIM_ROUTE = 0x1,  // Claim a route between two cities

    DRAW_BLIND_CARD, // Draw a card from the deck
    DRAW_CARD, // Draw a card from the visible cards

    DRAW_OBJECTIVES, // Draw 3 objectives
    CHOOSE_OBJECTIVES // Choose 1 to 3 objectives
} Action;


/* Different colors */
typedef enum {
    NONE = 0,       // only used when a route does not have a second color/track
    PURPLE = 1,
	WHITE = 2,
	BLUE = 3,
	YELLOW = 4,
	ORANGE = 5,
	BLACK = 6,
	RED = 7,
	GREEN = 8,
	LOCOMOTIVE = 9  // jocker, that can be used for any color
} CardColor;


/* define an objective
 * from a city to another city
 * and allow to win `score` points */
typedef struct {
    unsigned int from;
    unsigned int to;
    unsigned int score;
} Objective;


/* Data used to clam a route, from a city to another city
 * using a color, and some locomotive cards
 */
typedef struct {
    unsigned int from;
    unsigned int to;
    CardColor color;
    unsigned int nbLocomotives;
} ClaimRouteMove;


/* Data defining a move */
typedef struct {
    Action action;                      // One of Actions values

    union {
        ClaimRouteMove claimRoute;      // the route we claim
        CardColor drawCard;             // the card we draw
        bool chooseObjectives[3];       // the objectives we choose
    };
} MoveData;


/* Data returns after a move */
typedef struct MoveResult_ {
    MoveState state;            // tells if the move winning/losing/normal move

    union {
        CardColor card;             // card when we draw a blind card
        Objective objectives[3];    // objectives when we draw the ojbectives
    };
    bool replay;                    // true if the player plays once again

    char* opponentMessage;          // String containing a message send by the opponent
    char* message;                  // String containing a message send by the server
} MoveResult;


/* data returned when we call getBoardState
 * here the five face-up cards
 */
typedef struct {
        CardColor card[5]; // Visible cards
} BoardState;


/* game data, used to get the initial values of the board
 * the `trackData` is a raw array of (5 x number of tracks) integers
 * 	 Five integers are used to define a track:
 * 	  - (1) id of the 1st city
 * 	  - (2) id of the 2nd city
 * 	  - (3) length of the track (between 1 and 6)
 * 	  - (4) color of the track (MULTICOLOR if any color can be used)
 * 	  - (5) color of the 2nd track if the track is double (NONE if the track is not a double track) */
typedef struct {
    char* gameName; // String containing the game name
    int gameSeed; // Contain the seed used for the game board generation (if you didn't provide one)
    int starter; // Defines who start, 1 you and 2 opponent

    int nbCities; // Total number of cities in the map
    int nbTracks;   // total number of tracks
    int* trackData; // Track data, an array containing unformatted data about the trackstracks:
    CardColor cards[4];
} GameData;



/*

    Exposed functions

*/

/* -------------------------------------
 * Initialize connection with the server
 * This is the first function you should call, it will connect you to the server.
 * You need to provide the server address and the port to connect to.
 * This is a blocking function, it will wait until the connection is established, it may take some time.
 *
 * Parameters:
 * - address: (string) address of the server
 * - port: (int) port number used for the connection
 * - name: (string) your bot's name
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode connectToCGS(const char* address, unsigned int port, const char* name);


/* -------------------------------------
 * Send the game settings to the server in order to start a game
 * After sending your name you need to send game settings to the server to start a game.
 * You need to provide a GameSettings struct and a GameData struct to store the game data returned by the server.
 * You can use the GameSettingsDefaults and GameDataDefaults variables to initialize the struct with default values.
 * To fill the GameSettings struct you may want to use predefined constants available in codingGameServer.h.
 *
 * The fields `gameName` and `trackData` (of GameData) are allocated by the function, so they need to be freed by the user
 *
 * Parameters:
 * - gameSettings: (string) string defining the settings for the game
 * - gameData: (GameData*) store the game data
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode sendGameSettings(const char* gameSettings, GameData* gameData);


/* -------------------------------------
 * Get the move of the opponent
 * During a game this function is used to know what your opponent did during his turn.
 * You need to provide an empty MoveData struct and an empty MoveResult struct to store the move data returned by the server.
 * MoveData struct store the move your opponent did and MoveResult struct store the result of the move.
 *
 * The fields `opponentMessage` and `message` (of moveResult) are allocated by the function, so they need to be freed by the user
 *
 * Parameters:
 * - moveData: (GameSettings*) data defining the opponent's move
 * - moreResult: (MoveResult*) data returned after the move
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode getMove(MoveData* moveData, MoveResult* moveResult);


/* -------------------------------------
 * Send the move to the server
 * During a game this function is used to send your move to the server.
 * You need to provide a MoveData struct containing your move and an empty MoveResult struct to store the result of the
 * move returned by the server.
 *
 * The fields `opponentMessage` and `message` (of moveResult) are allocated by the function, so they need to be freed by the user
 *
 * Parameters:
 * - moveData: (GameSettings*) data defining our move
 * - moreResult: (MoveResult*) data returned after the move
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode sendMove(const MoveData *moveData, MoveResult* moveResult);


/* -------------------------------------
 * This function is used to get the current state of the board during a game.
 * It returns the 5 face-up cards
 *
 * Parameters:
 * - boardState: (BoardState*) the 5 face-up cards
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode getBoardState(BoardState* boardState);


/* -------------------------------------
 * This function is used to send a message to your opponent during a game.
 * You need to provide the message as a string. It should be less than 256 characters long.
 *
 * Parameters:
 * - message: (string) the message sent
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode sendMessage(const char* message);


/* -------------------------------------
 * This function is used to display the game board during a game.
 * It will print the colored board in the console.
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode printBoard();


/* -------------------------------------
 * This function prints the city name
 *
 * Parameters:
 * - cityId: (int) id of the city to be printed
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode printCity(unsigned int cityId);


/* -------------------------------------
 * This function is used to quit the currently running game.
 *
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode quitGame();


#endif
