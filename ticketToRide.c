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
const GameData GameDataDefaults = { "", 0, 0, 0, 0 };

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

    gameData->nbCities = getIntFromTokens(string, "nbCities", tokens, 19);
    gameData->nbTracks = getIntFromTokens(string, "nbTracks", tokens, 19);

    char* tracksArray = getStringFromTokens(string, "trackData", tokens, 19);

    int cardsIndex = searchInTokens(string, "playerCard", tokens, 19);
    int deckCards = 0;
    for(int i = 0; i < 10 * 3; i += 3) {
        int cardCount = (unsigned int) atoi(string+tokens[cardsIndex + 1].start + i);
        
        if(cardCount > 0) {
            CardColor card = i / 3;
            gameData->cards[deckCards] = card;

            deckCards += 1;
        }
    }
    
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
            dataLength = sprintf(data, "{ 'action': 'sendMove', 'move': %d, 'card': %d }", DRAW_CARD, moveData->drawCard.card);
            break;
        case DRAW_OBJECTIVES:
            dataLength = sprintf(data, "{ 'action': 'sendMove', 'move': %d }", DRAW_OBJECTIVES);
            break;
        case CHOOSE_OBJECTIVES:
            dataLength = sprintf(data, "{ 'action': 'sendMove', 'move': %d, 'selectCard': [%d, %d, %d] }",
                CHOOSE_OBJECTIVES, moveData->chooseObjectve.selectCard[0], moveData->chooseObjectve.selectCard[1], moveData->chooseObjectve.selectCard[2]);
            break;
        default:
            return -1;
    }

    return dataLength;
}

ResultCode unpackGetMoveData(char* string, jsmntok_t* tokens, MoveData* moveData, MoveResult* moveResult) {
    // Load received data into struct
    moveData->action = (Action) atoi(&string[tokens[4].start]);
    moveResult->state = (unsigned int) atoi(&string[tokens[6].start]);

    int blockLength = tokens[8].end - tokens[8].start + 1;
    char* opponentMessage = (char *) malloc(blockLength * sizeof(char));
    if(opponentMessage == NULL) return MEMORY_ALLOCATION_ERROR;

    sprintf(opponentMessage, "%.*s", blockLength - 1, &string[tokens[8].start]);
    moveResult->opponentMessage = opponentMessage;;

    int blockLength2 = tokens[10].end - tokens[10].start + 1;
    char* message = (char *) malloc(blockLength2 * sizeof(char));
    if(message == NULL) return MEMORY_ALLOCATION_ERROR;

    sprintf(message, "%.*s", blockLength2 - 1, &string[tokens[10].start]);
    moveResult->message = message;

    switch (moveData->action) {
        case CLAIM_ROUTE:
            moveData->claimRoute.from = atoi(&string[tokens[12].start]);
            moveData->claimRoute.to = atoi(&string[tokens[14].start]);
            moveData->claimRoute.color = (CardColor) atoi(&string[tokens[16].start]);
            moveData->claimRoute.nbLocomotives = atoi(&string[tokens[18].start]);
            break;
        case DRAW_CARD:
            moveData->drawCard.card = (CardColor) atoi(&string[tokens[12].start]);
            break;
        case CHOOSE_OBJECTIVES:
            moveData->chooseObjectve.selectCard[0] = (unsigned int) atoi(&string[tokens[12].start]);
            moveData->chooseObjectve.selectCard[1] = (unsigned int) atoi(&string[tokens[14].start]);
            moveData->chooseObjectve.selectCard[2] = (unsigned int) atoi(&string[tokens[16].start]);
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
    moveResult->state = (Action) atoi(&string[tokens[2].start]);

    int blockLength = tokens[6].end - tokens[6].start + 1;
    char* opponentMessage = (char *) malloc(blockLength * sizeof(char));
    if(opponentMessage == NULL) return MEMORY_ALLOCATION_ERROR;

    sprintf(opponentMessage, "%.*s", blockLength - 1, &string[tokens[6].start]);
    moveResult->opponentMessage = opponentMessage;

    int blockLength2 = tokens[8].end - tokens[8].start + 1;
    char* message = (char *) malloc(blockLength2 * sizeof(char));
    if(message == NULL) return MEMORY_ALLOCATION_ERROR;

    sprintf(message, "%.*s", blockLength2 - 1, &string[tokens[8].start]);
    moveResult->message = message;

    switch (moveResult->state) {
        case DRAW_BLIND_CARD:
            moveResult->card = (CardColor) atoi(&string[tokens[10].start]);
            break;
        case DRAW_OBJECTIVES:
            for(int i = 0; i < 3; i++) {
                moveResult->objectives[i].from = atoi(&string[tokens[10 + (i * 6)].start]);
                moveResult->objectives[i].to = atoi(&string[tokens[12 + (i * 6)].start]);
                moveResult->objectives[i].score = atoi(&string[tokens[14 + (i * 6)].start]);
            }
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
    for(int i = 0; i < 5; i++)
        boardState->card[i] = (CardColor) atoi(&string[tokens[4 + i * 2].start]);

    return ALL_GOOD;
}