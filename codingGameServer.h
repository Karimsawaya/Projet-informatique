/*

    API for the Coding Game Server

    Require codingGameServer.h, lib/json.h ang gameHeaders/[game name].h to works with.

    Authors: Valentin Le Lièvre
    Licence: GPL

    Copyright 2025 Valentin Le Lièvre

    ---------------------------------

    Note: See codingGameServer.h for documentation

*/

#ifndef API_H
#define API_H

#include "ticketToRide.h"

// some intern definitions
#define MAX_TIMEOUT 60 // in seconds
#define MIN_TIMEOUT 5

#define MAX_SEED 10000

#define MAX_USERNAME_LENGTH 100
#define MAX_MESSAGE_LENGTH 256

#define GAME_SETTINGS_MAX_JSON_LENGTH 250
#define PACKED_DATA_MAX_SIZE 400

#define GET_MOVE_RESPONSE_JSON_SIZE 19
#define SEND_MOVE_RESPONSE_JSON_SIZE 29

#define BOARD_STATE_RESPONSE_JSON_SIZE 13

#define FIRST_MSG_LENGTH 6

// Json messages size, (nb of key * 2) + 1
#define SERVER_ACKNOWLEDGEMENT_JSON_SIZE 5
#define GAME_SETTINGS_ACKNOWLEDGEMENT_JSON_SIZE 19


// Some prototypes
int packGameSettings(char* data, GameSettings gameSettings);
ResultCode unpackGameSettingsData(char *string, jsmntok_t *tokens, GameData *gameData);
ResultCode unpackGetMoveData(char* string, jsmntok_t* tokens, MoveData* moveData, MoveResult* moveResult);
int packSendMoveData(char* data, const MoveData *moveData);
ResultCode unpackSendMoveResult(char *string, jsmntok_t *tokens, MoveResult *moveResult);
ResultCode unpackGetBoardState(char *string, jsmntok_t *tokens, BoardState *boardState);
static ResultCode connectToSocket(const char *address, unsigned int port, int adrType);
static ResultCode dnsSearch(const char *domain, char** ipAddress, int* adrType);
static ResultCode sendData(const char *data, unsigned int dataLength);
static ResultCode getServerResponse(char **string, jsmntok_t **tokens, int nbMaxTokens);
static ResultCode getData(char **string, int *stringLength);
static ResultCode readNByte(char **buffer, int nbByte);
static int getIntegerLength(int value);
static int isValidIpAddress(const char *ipAddress);
ResultCode printError(const char* function, ResultCode code, const char* message, ...);
void printDebugMessage(const char* function, unsigned int level, const char* message, ...);
int getIntFromTokens(const char *string, const char* prop, const jsmntok_t *tokens, int nbMaxTokens);
char* getStringFromTokens(const char *string, const char* prop, const jsmntok_t *tokens, int nbMaxTokens);
int searchInTokens(const char *string, const char* prop, const jsmntok_t *tokens, int nbMaxTokens);

#endif