
#ifndef __CLIENT_API_H__
#define __CLIENT_API_H__

#include "stdlib.h"

/*
 *   Structure and type definitions
*/


/* `ResultCode` is used to indicate the failure/success of a function. Every important function returns this code */
typedef enum {
    PARAM_ERROR = 0x10,
    SERVER_ERROR = 0x20,
    OTHER_ERROR = 0x30,
    MEMORY_ALLOCATION_ERROR = 0x40,
    ALL_GOOD = 0x50
} ResultCode;


/* Debug level
 * in some rare cases, it could be interesting to display some debug messages (log). This can be done by changing the
 * value of a specific variable named `DEBUG_LEVEL`
 * You can declare an extern variable with this name
 * `extern DebugLevel DEBUG_LEVEL;`
 * And then set the level at appropriate message
 * `DEBUG_LEVEL = MESSAGE;`
 */
typedef enum {
    NO_DEBUG = 0x0,
    MESSAGE,                // display some messages, stop at errors
    DEBUG,                  // display debug messages
    INTERN_DEBUG            // display intern debug messages, like the messages exchanged between the client and the server
} DebugLevel;


/* different possible states for the move */
typedef enum {
    NORMAL_MOVE = 0,          // regular move, nobody loose or win
    LOSING_MOVE = -1,         // the player looses the game
    WINNING_MOVE = 1,         // the player wins the game
} MoveState;


/* intern constants */

#define MAX_GET_MOVE 128	    	/* maximum size of the string representing a move */
#define MAX_MESSAGE 1024			/* maximum size of the message move */

/* prototypes */
void connectToCGSServer(const char* fct, const char* serverName, unsigned int port, const char* name);
void closeCGSConnection(const char* fct);
void waitForGame(const char* fct, const char* gameType, char* gameName, char* data);
int getGameData(const char* fct, char* data, size_t ndata);
MoveState getCGSMove(const char* fct, char* move ,char* msg);
MoveState sendCGSMove( const char* fct, char* move, char* answer);
void printCGSGame(const char* fct);
void sendCGSComment(const char* fct, const char* comment);



#endif
