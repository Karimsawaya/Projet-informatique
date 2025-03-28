#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// This define is used by the JSON library
#define JSMN_HEADER
#include "json.h"

// Game headers
#include "ticketToRide.h"
#include "codingGameServer.h"



/*
    Default values for struct
    You can use those variables to initialize struct with default values
*/

const GameSettings GameSettingsDefaults = { TRAINING, RANDOM_PLAYER, 10, 0, 0, 0 };
const GameData GameDataDefaults = { "", 0, 0, 0, 0, NULL, {0,0,0,0} };

unsigned int nbCities = 0;
char** cityNames = NULL;

/*
    Functions
*/

int packGameSettings(char* data, GameSettings gameSettings) {
    int dataLength = sprintf(data, "{ 'gameType': %d, 'botId': %d, 'timeout': %d, 'starter': %d, 'seed': %d, 'reconnect': %d }", gameSettings.gameType, gameSettings.botId, gameSettings.timeout, gameSettings.starter, gameSettings.seed, gameSettings.reconnect);
    return dataLength;
}


ResultCode unpackGameSettingsData(char *string, jsmntok_t *tokens, GameData *gameData) {
    // Print string
    printDebugMessage(__FUNCTION__, INTERN_DEBUG, "Received data: %s", string);

    nbCities = gameData->nbCities = getIntFromTokens(string, "nbCities", tokens, 19);
    gameData->nbTracks = getIntFromTokens(string, "nbTracks", tokens, 19);

    // // retrieve the tracks data
    char* tracksArray = getStringFromTokens(string, "trackData", tokens, 19);
    char* p = tracksArray;
    int nbchar;

    gameData->trackData = (int*) malloc(sizeof(int) * gameData->nbTracks * 5);
    if (!gameData->trackData) return MEMORY_ALLOCATION_ERROR;
    int* ptr = gameData->trackData;
    for(int i=0; i < gameData->nbTracks; i++){
        sscanf(p, "%d %d %d %d %d %n", ptr, ptr+1, ptr+2, ptr+3, ptr+4, &nbchar);
        ptr += 5;
        p += nbchar;
    }
    free(tracksArray);

    // Retrieve the 4 cards
    char* cardsArray = getStringFromTokens(string, "playerCards", tokens, 19);
    sscanf(cardsArray, "%d %d %d %d", (int*)gameData->cards, (int*)gameData->cards+1, (int*)gameData->cards+2, (int*)gameData->cards+3);
    free(cardsArray);

    // retrieve the city names
    char* cities = getStringFromTokens(string, "cities", tokens, 19);
    cityNames = (char **)malloc(gameData->nbCities * sizeof(char *));
    if (!cityNames) return MEMORY_ALLOCATION_ERROR;
    char* start = p = cities;
    int index = 0;

    while (1) {
        if (*p == '|' || *p == '\0') {
            size_t len = p - start;
            cityNames[index] = (char *)malloc(len + 1); // +1 for null terminator
            if (!cityNames[index]) return MEMORY_ALLOCATION_ERROR;
            strncpy(cityNames[index], start, len);
            cityNames[index][len] = '\0'; // Null-terminate the string
            index++;
            if (*p == '\0')
                break; // End of string
            start = p + 1;
        }
        p++;
    }
    free(cities);
    return ALL_GOOD;
}

int packSendMoveData(char* data, const MoveData *moveData) {
    int dataLength = 0;

    switch (moveData->action) {
        case CLAIM_ROUTE:
            dataLength = sprintf(data, "{ 'action': 'sendMove', 'move': %d, 'from': %d, 'to': %d, 'color': %d, 'nbLocomotives': %d }",
                CLAIM_ROUTE, moveData->claimRoute.from, moveData->claimRoute.to, moveData->claimRoute.color, moveData->claimRoute.nbLocomotives);
            break;
        case DRAW_BLIND_CARD:
            dataLength = sprintf(data, "{ 'action': 'sendMove', 'move': %d }", DRAW_BLIND_CARD);
            break;
        case DRAW_CARD:
            dataLength = sprintf(data, "{ 'action': 'sendMove', 'move': %d, 'card': %d }", DRAW_CARD, moveData->drawCard);
            break;
        case DRAW_OBJECTIVES:
            dataLength = sprintf(data, "{ 'action': 'sendMove', 'move': %d }", DRAW_OBJECTIVES);
            break;
        case CHOOSE_OBJECTIVES:
            dataLength = sprintf(data, "{ 'action': 'sendMove', 'move': %d, 'selectCard': [%d, %d, %d] }",
                CHOOSE_OBJECTIVES, moveData->chooseObjectives[0], moveData->chooseObjectives[1], moveData->chooseObjectives[2]);
            break;
        default:
            return -1;
    }

    return dataLength;
}

ResultCode unpackGetMoveData(char* string, jsmntok_t* tokens, MoveData* moveData, MoveResult* moveResult) {
    // Load received data into struct
    moveData->action = (Action) getIntFromTokens(string, "move", tokens, 19);
    moveResult->state = (unsigned int) getIntFromTokens(string, "returnCode", tokens, 19);
    moveResult->opponentMessage = getStringFromTokens(string, "op_message", tokens, 19);;
    moveResult->message = getStringFromTokens(string, "message", tokens, 19);

    switch (moveData->action) {
        case CLAIM_ROUTE:
            moveData->claimRoute.from = getIntFromTokens(string, "from", tokens, 19);
            moveData->claimRoute.to = getIntFromTokens(string, "to", tokens, 19);
            moveData->claimRoute.color = (CardColor) getIntFromTokens(string, "color", tokens, 19);
            moveData->claimRoute.nbLocomotives = getIntFromTokens(string, "nbLocomotives", tokens, 19);
            break;
        case DRAW_CARD:
            moveData->drawCard = (CardColor) getIntFromTokens(string, "cardColor", tokens, 19);;
            break;
        case CHOOSE_OBJECTIVES:
            moveData->chooseObjectives[0] = getIntFromTokens(string, "keepedObjectives1", tokens, 19);
            moveData->chooseObjectives[1] = getIntFromTokens(string, "keepedObjectives2", tokens, 19);
            moveData->chooseObjectives[2] = getIntFromTokens(string, "keepedObjectives3", tokens, 19);
            break;
        case DRAW_BLIND_CARD:
            // No additional data to unpack
            break;
        case DRAW_OBJECTIVES:
            // No additional data to unpack
            break;
        default:
            return PARAM_ERROR;
    }

    return ALL_GOOD;
}

ResultCode unpackSendMoveResult(char *string, jsmntok_t *tokens, MoveResult *moveResult) {
    Action moveAction = (Action) getIntFromTokens(string, "move", tokens, 29);
    moveResult->state = (unsigned int) getIntFromTokens(string, "returnCode", tokens, 29);
    moveResult->opponentMessage = getStringFromTokens(string, "op_message", tokens, 29);;
    moveResult->message = getStringFromTokens(string, "message", tokens, 29);

    switch (moveAction) {
        case DRAW_BLIND_CARD:
            moveResult->card = (CardColor) getIntFromTokens(string, "cardColor", tokens, 29);;
            break;
        case DRAW_OBJECTIVES:
                // TODO: ugly hard-coded message
                moveResult->objectives[0].from = getIntFromTokens(string, "from1", tokens, 29);
                moveResult->objectives[0].to = getIntFromTokens(string, "to1", tokens, 29);
                moveResult->objectives[0].score = getIntFromTokens(string, "score1", tokens, 29);
                moveResult->objectives[1].from = getIntFromTokens(string, "from2", tokens, 29);
                moveResult->objectives[1].to = getIntFromTokens(string, "to2", tokens, 29);
                moveResult->objectives[1].score = getIntFromTokens(string, "score2", tokens, 29);
                moveResult->objectives[2].from = getIntFromTokens(string, "from3", tokens, 29);
                moveResult->objectives[2].to = getIntFromTokens(string, "to3", tokens, 29);
                moveResult->objectives[2].score = getIntFromTokens(string, "score3", tokens, 29);
            break;
        case CLAIM_ROUTE:
            // No additional data to unpack
            break;
        case CHOOSE_OBJECTIVES:
            // No additional data to unpack
            break;
        case DRAW_CARD:
            // No additional data to unpack
            break;
        default:
            return PARAM_ERROR;
    }

    return ALL_GOOD;
}

ResultCode unpackGetBoardState(char* string, jsmntok_t* tokens, BoardState* boardState) {
    // retrieve the 5 board cards
    char* cardsArray = getStringFromTokens(string, "cards", tokens, 5);
    sscanf(cardsArray, "%d %d %d %d %d", (int*)boardState->card, (int*)boardState->card+1, (int*)boardState->card+2, (int*)boardState->card+3, (int*)boardState->card+4);
    free(cardsArray);
    return ALL_GOOD;
}

// Prints the city name
ResultCode printCity(unsigned int cityId) {
    // check the parameters
    if (cityId >= nbCities) return PARAM_ERROR;
    // print the name
    printf("%s", cityNames[cityId]);
    return ALL_GOOD;
}

void deallocGameData() {
    for (int i = 0; i < nbCities; i++) {
        free(cityNames[i]);
    }
    free(cityNames);
}