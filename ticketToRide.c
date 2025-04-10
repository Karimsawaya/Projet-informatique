
#include <stdio.h>
#include <string.h>
#include "ticketToRide.h"
#include "clientAPI.h"


/* global variables used in intern, so the user do not have to pass them once again) */
int nbTr;        		/* number of tracks */
int nbC;				/* number of cities */
char** cityNames;		/* array of city names (used by printCity) */
CardColor faceUp[5];   /* store the face up cards returned by the get/sendMove */

/* -----------------------
 * Dummy function that does
 * a string copy (exactly as strcpy)
 * but replace the '_' by ' ' in the same time it does the copy
 */
void strCpyReplace(char* dest, const char* src)
{
	while(*src) {
		if (*src != '_')
			*dest++ = *src;
		else
			*dest++ = ' ';
		src++;
	}
}


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
ResultCode connectToCGS(const char* address, unsigned int port, const char* name){
    connectToCGSServer(__FUNCTION__, address, port, name);
    return ALL_GOOD;
}


/* -------------------------------------
 * Send the game settings to the server in order to start a game
 * After connecting, you need to send game settings to the server to start a game.
 * You need to provide a string for the game setting and a GameData struct to store the game data returned by the server.
  *
 * The fields `gameName` and `trackData` (of GameData) are allocated by the function, so they need to be freed by the user
 *
 * Parameters:
* - gameType: string (max 200 characters) type of the game we want to play (empty string for regular game)
 *             "TRAINING xxxx" to play with the bot xxxx
 *             "TOURNAMENT xxxx" to join the tournament xxxx
 *     gameType can also contain extra data in form "key1=value1 key2=value1 ..." to provide options (to bots)
 *     invalid keys are ignored, invalid values leads to error
 *     the options are:
 *        - 'timeout': allows to define the timeout when training (in seconds)
 *        - 'seed': allows to set the seed of the random generator
 *        - 'start': allows to set who starts ('0' to begin, '1' otherwise)
 *        - 'map': allows to choose a map ('USA', 'small' or 'Europe')
 *     the following bots are available:
 *        - DO_NOTHING (stupid bot what withdraw cards)
 *        - PLAY_RANDOM (bot that plays randomly)
 *        - NICE_BOT (bot that plays reasonably well, but not too well)
 * - gameData: (GameData*) store the game data
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode sendGameSettings(const char* gameSettings, GameData* gameData){
    char data[4096];
    int nbchar;
	char *p, **name;
	char city[20];

    /* wait for a game  and parse the data*/
	char gameName[50];
	waitForGame(__FUNCTION__, gameSettings, gameName, data);
	sscanf(data, "%d %d", &nbC, &nbTr);
	gameData->nbTracks = nbTr;
	gameData->nbCities = nbC;
	cityNames = (char**) malloc(nbC*sizeof(char*));
	gameData->gameName = (char*)malloc((strlen(gameName)+1)*sizeof(char));
	strcpy(gameData->gameName, gameName);
	/* get the seed from the name */
	char seedstr[7];
	strncpy(seedstr, gameName, 6);
	seedstr[6] = '\0';
	sscanf(seedstr, "%x", &gameData->gameSeed);

	/* wait for the game data */
	gameData->starter = getGameData( __FUNCTION__, data, 4096);

	/* copy the cities' names */
	p = data;
	name = cityNames;
	for(int i=0; i < nbC; i++){
		sscanf(p, "%s%n", city, &nbchar);
		p += nbchar;
		*name = (char*) malloc(strlen(city)+1);
		strCpyReplace(*(name++), city);
	}

	/* copy the data in the tracks array */
	gameData->trackData = (int*) malloc(sizeof(int) * gameData->nbTracks * 5);
	int* tracks = gameData->trackData;
	if (!gameData->trackData) return MEMORY_ALLOCATION_ERROR;
	for(int i=0; i < nbTr; i++){
		sscanf(p, "%d %d %d %d %d %n", tracks, tracks+1, tracks+2, tracks+3, tracks+4, &nbchar);
		tracks += 5;
		p += nbchar;
	}

	/* get the 5 face up cards, but ignore them */
	sscanf(p, "%d %d %d %d %d %n", (int*)faceUp, (int*)faceUp+1, (int*)faceUp+2, (int*)faceUp+3, (int*)faceUp+4, &nbchar);
	p += nbchar;
	/* get the 4 initial cards */
	sscanf(p, "%d %d %d %d", (int*)gameData->cards, (int*)gameData->cards+1, (int*)gameData->cards+2, (int*)gameData->cards+3);

	return ALL_GOOD;
}



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
ResultCode getMove(MoveData* moveData, MoveResult* moveResult){
	char moveStr[MAX_GET_MOVE];
	char msg[MAX_MESSAGE];
	int obj[3];
	char* p;
	unsigned int nbchar;
	int replay;


	/* get the move */
	moveResult->state = getCGSMove(__FUNCTION__, moveStr, msg);
	moveResult->replay = false;

	/* extract result */
	if (moveResult->state == NORMAL_MOVE) {
		sscanf(moveStr, "%d%n", (int*) &moveData->action, &nbchar);
		p = moveStr + nbchar;
		if (moveData->action == CLAIM_ROUTE) {
			sscanf(p, "%d %d %d %d", &moveData->claimRoute.from, &moveData->claimRoute.to, (int*)&moveData->claimRoute.color, &moveData->claimRoute.nbLocomotives);
		}
		else if (moveData->action == DRAW_CARD) {
			sscanf(msg, "%d %d %d %d %d %d %d", &replay, (int*) &moveData->drawCard, (int*) faceUp, (int*) faceUp+1, (int*) faceUp+2, (int*) faceUp+3, (int*) faceUp+4);
			moveResult->replay =  (bool) replay;
		}
		else if (moveData->action == DRAW_BLIND_CARD){
			sscanf(msg, "%d", &replay);
			moveResult->replay = (bool) replay;
			moveResult->card = NONE;		/* we don't know which card the opponent has */
		}
		else if (moveData->action == DRAW_OBJECTIVES) {
			moveResult->replay = true;
		}
		else if (moveData->action == CHOOSE_OBJECTIVES) {
			sscanf(p, "%d %d %d", obj, obj + 1, obj + 2);
			for(int i=0;i<3;i++)
			    moveData->chooseObjectives[i] = (bool) obj[i];
		}
	}

    //TODO: get the messages
    moveResult->message = NULL;
    moveResult->opponentMessage = NULL;

	return ALL_GOOD;


}



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
ResultCode sendMove(const MoveData *moveData, MoveResult* moveResult){
    char msg[256];
	char answer[MAX_MESSAGE], *str = answer;
	int nbchar;
	int replay;

	// TODO: manage messages
	moveResult->message = NULL;
	moveResult->opponentMessage = NULL;
	moveResult->replay = false;
    // send the appropriate message
    switch(moveData->action){

        case CLAIM_ROUTE:
            sprintf(msg, "1 %d %d %d %d", moveData->claimRoute.from, moveData->claimRoute.to, moveData->claimRoute.color, moveData->claimRoute.nbLocomotives);
	        moveResult->state = sendCGSMove(__FUNCTION__, msg, answer);
	        break;

	    case DRAW_BLIND_CARD:
	        moveResult->state = sendCGSMove(__FUNCTION__, "2", answer);
        	/* get card drawn */
	        if (moveResult->state == NORMAL_MOVE) {
		        sscanf(answer, "%d %d", &replay, (int*)&moveResult->card);
	        	moveResult->replay = replay;
	        }
		    break;

		case DRAW_CARD:
			sprintf(msg, "3 %d", moveData->drawCard);
	        moveResult->state = sendCGSMove(__FUNCTION__, msg, answer);
        	if (moveResult->state == NORMAL_MOVE) {
        		sscanf(answer, "%d %d %d %d %d %d", &replay, (int*)faceUp, (int*)faceUp+1, (int*)faceUp+2, (int*)faceUp+3, (int*)faceUp+4);
        		moveResult->replay = replay;
        	}
		    break;

		case DRAW_OBJECTIVES:
			moveResult->state = sendCGSMove(__FUNCTION__, "4", answer);
            if (moveResult->state == NORMAL_MOVE) {
                Objective *p = moveResult->objectives;
                for (int i = 0; i < 3; i++, p++) {
                    sscanf(str, "%d %d %d%n", &p->from, &p->to, &p->score, &nbchar);
                    str += nbchar;
                }
            	moveResult->replay = true;
            }
            break;

        case CHOOSE_OBJECTIVES:
            sprintf(msg, "5 %d %d %d", (int) moveData->chooseObjectives[0], (int) moveData->chooseObjectives[1], (int) moveData->chooseObjectives[2]);
	        moveResult->state = sendCGSMove(__FUNCTION__, msg, answer);
            break;
    }

    return ALL_GOOD;

}


/* -------------------------------------
 * This function is used to get the current state of the board during a game.
 * It returns the 5 face-up cards
 *
 * Parameters:
 * - boardState: (BoardState*) the 5 face-up cards
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode getBoardState(BoardState* boardState){
    for(int i=0;i<5;i++)
        boardState->card[i] = faceUp[i];
    return ALL_GOOD;
}

/* -------------------------------------
 * This function is used to send a message to your opponent during a game.
 * You need to provide the message as a string. It should be less than 256 characters long.
 *
 * Parameters:
 * - message: (string) the message sent
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode sendMessage(const char* message){
    sendCGSComment( __FUNCTION__, message);
    return ALL_GOOD;
}


/* -------------------------------------
 * This function is used to display the game board during a game.
 * It will print the colored board in the console.
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode printBoard(){
    printCGSGame(__FUNCTION__);
    return ALL_GOOD;
}


/* -------------------------------------
 * This function prints the city name
 *
 * Parameters:
 * - cityId: (int) id of the city to be printed
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode printCity(unsigned int cityId){
	printf("%s", cityNames[cityId]);
	return ALL_GOOD;
}


/* -------------------------------------
 * This function is used to quit the currently running game.
 *
 *
 * Returns the error code (ALL_GOOD if everything is ok) */
ResultCode quitGame(){
	/* free the data */
	char** p = cityNames;
	for(int i=0; i<nbC; i++)
		free(*p++);
	free(cityNames);
	/* close the connection */
	closeCGSConnection(__FUNCTION__);

	return ALL_GOOD;
}