/*

    API for the Coding Game Server

    Require codingGameServer.h, lib/json.h ang gameHeaders/[game name].h to works with.

    Authors: Valentin Le Lièvre
    Licence: GPL

    Copyright 2025 Valentin Le Lièvre

    ---------------------------------

    Note: See codingGameServer.h for documentation

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

// Socket headers
// Those libs are available on Linux (including WSL) and Mac but not Windows
// If you use windows you may want to use winsock.h and rename functions like inet_pton to match headers function names

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

// External JSON library
#include "json.h"

// Game headers
#include "ticketToRide.h"
#include "codingGameServer.h"

/*
    Global vars
*/

int SOCKET = -1;                  // socket descriptor
int DEBUG_LEVEL = NO_DEBUG;      // Set to 1 to enable debug mode, 0 to disable (use `extern int debug = 1;`)


/*

    Exposed functions

    Those are the function you can use to interact with the server

*/

// This is the first function you should call, it will connect you to the server.
// You need to provide the server address and the port to connect to.
// This is a blocking function, it will wait until the connection is established, it may take some time.

ResultCode connectToCGS(const char *address, unsigned int port) {
    ResultCode result;
    if(port<1000)
        return printError(__FUNCTION__, PARAM_ERROR, "Invalid port value");

    char* ipaddress = NULL;
    int adrType = 0;

    // Verify provided IP address and it's type: IPV4 or IPV6
    int ipVerificationResult = isValidIpAddress(address);

    if(ipVerificationResult <= 0) {
        // Invalid IP, user might have used a domain name instead of an IP address
        if((result = dnsSearch(address, &ipaddress, &adrType)) == ALL_GOOD) {
            char* addressTypeName = (char *) malloc(5 * sizeof(char));
            if(addressTypeName == NULL)
                return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, "");

            sprintf(addressTypeName, (adrType == AF_INET)? "IPv4": "IPv6");
            
            printDebugMessage(__FUNCTION__, MESSAGE, "Domain name %s resolved into an %s address: %s", address, addressTypeName, ipaddress);
            free(addressTypeName);
        } else {
            return printError(__FUNCTION__, result,  "Domain name %s failed to resolve into an IP address", address);
        }
    } else adrType = ipVerificationResult;

    /* connect to Socket */

    if(ipaddress != NULL) {
        result = connectToSocket(ipaddress, port, adrType);
        free(ipaddress);
    }
    else {
        result = connectToSocket(address, port, adrType);
    }
    if (result == ALL_GOOD)
        printDebugMessage(__FUNCTION__, DEBUG, "Successfully connected to %s", address);
    return result;
}

// After connecting to the server you need to send your name to the server. It will be used to uniquely identify you.
// You need to provide your name as a string. It should be less than 90 characters long.

ResultCode sendName(const char *name) {
    ResultCode result;
    // Check user's provided data, max name length is 90 characters
    if(strlen(name) >= MAX_USERNAME_LENGTH)
        return printError(__FUNCTION__, PARAM_ERROR, "Name too long");

    // Parse data into json string
    char* data = (char *) malloc((MAX_USERNAME_LENGTH + 20) * sizeof(char));
    if(data == NULL)
        return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, "");

    // Fill data with name
    int dataLength = sprintf(data, "{ 'name': '%s' }", name);

    // Send data and check for success
    if((result=sendData(data, dataLength)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to send data");

    free(data);

    // Get server acknowledgement
    char* string;
    if((result=getServerResponse(&string, NULL, SERVER_ACKNOWLEDGEMENT_JSON_SIZE)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Server response failed");
    free(string);

    // Return success
    return ALL_GOOD;
}

// After sending your name you need to send game settings to the server to start a game.
// You need to provide a GameSettings struct and a GameData struct to store the game data returned by the server.
// You can use the GameSettingsDefaults and GameDataDefaults variables to initialize the struct with default values.
// To fill the GameSettings struct you may want to use predefined constants available in codingGameServer.h.

ResultCode sendGameSettings(GameSettings gameSettings, GameData* gameData) {
    ResultCode result;
    // Check user's provided data
    if(gameSettings.gameType >= GamesTypesMax || gameSettings.gameType <= 0)
        return printError(__FUNCTION__, PARAM_ERROR, "Invalid game type");
    if(gameSettings.botId >= BotsNamesMax || gameSettings.botId <= 0)
        return printError(__FUNCTION__, PARAM_ERROR, "Invalid bot id");
    if(gameSettings.timeout > MAX_TIMEOUT || gameSettings.timeout < MIN_TIMEOUT)
        return printError(__FUNCTION__, PARAM_ERROR, "Invalid timeout value");
    if(gameSettings.starter != 0 && gameSettings.starter != 1 && gameSettings.starter != 2)
        return printError(__FUNCTION__, PARAM_ERROR, "Invalid starter value");
    if(gameSettings.seed > MAX_SEED)
        return printError(__FUNCTION__, PARAM_ERROR, "Invalid seed value");
    
    // Parse data into json string
    char* data = (char *) malloc(GAME_SETTINGS_MAX_JSON_LENGTH * sizeof(char));
    if(data == NULL)
        return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR,"");

    int dataLength = packGameSettings(data, gameSettings);

    // Send data and check for success
    if((result = sendData(data, dataLength)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to send data");
    free(data);

    // Get server acknowledgement
    char* string;
    jsmntok_t* tokens;
     if((result=getServerResponse(&string, &tokens, GAME_SETTINGS_ACKNOWLEDGEMENT_JSON_SIZE)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Server response failed");


    // Set struct from param to defaults values
    *gameData = GameDataDefaults;

    // get nameName, gameSeed and starter
    gameData->gameName = getStringFromTokens(string, "gameName", tokens, GAME_SETTINGS_ACKNOWLEDGEMENT_JSON_SIZE);
    gameData->gameSeed = getIntFromTokens(string, "gameSeed", tokens, GAME_SETTINGS_ACKNOWLEDGEMENT_JSON_SIZE);
    gameData->starter = getIntFromTokens(string, "starter", tokens, GAME_SETTINGS_ACKNOWLEDGEMENT_JSON_SIZE);

/*
    // Load received data into struct
    int blockLength = tokens[4].end - tokens[4].start + 1;
    char* gameName = (char *) malloc(blockLength * sizeof(char));
    if(gameName == NULL)
        return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, "");

    sprintf(gameName, "%.*s", blockLength - 1, &string[tokens[4].start]);
    gameData->gameName = gameName;

    // Data is in specific order
    gameData->gameSeed = atoi(&string[tokens[6].start]);
    gameData->starter = atoi(&string[tokens[8].start]);
    gameData->nbElements = atoi(&string[tokens[14].start]);

    blockLength = tokens[16].end - tokens[16].start + 1;
    int* boardData = (int *) malloc(blockLength * sizeof(int));
    if(boardData == NULL)
        return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, "");

    int i = 0; int j = 0;
    while((j + i) < blockLength - 1) {
        boardData[i] = atoi(&string[tokens[16].start + j + i]);
        j += getIntegerLength(boardData[i]);
        i ++;
    }

    gameData->boardData = boardData;
*/
    if((result=unpackGameSettingsData(string, tokens, gameData)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to unpack game settings");

    free(string);

    // Return success
    return ALL_GOOD;
}


// During a game this function is used to know what your opponent did during his turn.
// You need to provide an empty MoveData struct and an empty MoveResult struct to store the move data returned by the server.
// MoveData struct store the move your opponent did and MoveResult struct store the result of the move.
ResultCode getMove(MoveData* moveData, MoveResult* moveResult) {
    ResultCode result;
    // Parse data into json string
    char* data = "{ 'action': 'getMove' }";

    // Send data and check for success
    if((result = sendData(data, strlen(data))) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to send data");

    // Get server acknowledgement
    char* string;
    jsmntok_t* tokens;
    if((result=getServerResponse(&string, &tokens, SEND_MOVE_RESPONSE_JSON_SIZE)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Server response failed");

    // Call the function related to the selected game to properly unpack the data
    if((result=unpackGetMoveData(string, tokens, moveData, moveResult)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to unpack move data");

    free(string);
    free(tokens);

    // Return success
    return ALL_GOOD;
}

// During a game this function is used to send your move to the server.
// You need to provide a MoveData struct containing your move and an empty MoveResult struct to store the result of the move returned by the server.

ResultCode sendMove(const MoveData *moveData, MoveResult* moveResult) {
    ResultCode result;
    // Parse data into json string
    char* data = (char *) malloc(PACKED_DATA_MAX_SIZE * sizeof(char));
    if(data == NULL)
        return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, "");

    // Call the function related to the selected game to properly pack the data
    int dataLength = packSendMoveData(data, moveData);
    if(dataLength == -1)
        return printError(__FUNCTION__, OTHER_ERROR, "Failed to send move data");

    // Send data and check for success
    if((result = sendData(data, dataLength)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to send data");
    free(data);

    // Get server acknowledgement
    char* string;
    jsmntok_t* tokens;

    if((result=getServerResponse(&string, &tokens, SEND_MOVE_RESPONSE_JSON_SIZE)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Server response failed");

    if((result=unpackSendMoveResult(string, tokens, moveResult)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to unpack move data");

    free(string);
    free(tokens);

    // Return success
    return ALL_GOOD;
}

ResultCode getBoardState(BoardState* boardState) {
    ResultCode result;
    // Parse data into json string
    char* data = "{ 'action': 'getBoardState' }";

    // Send data and check for success
    if((result=sendData(data, strlen(data))) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to send data");

    // Get server acknowledgement
    char* string;
    jsmntok_t* tokens;
    if((result=getServerResponse(&string, &tokens, BOARD_STATE_RESPONSE_JSON_SIZE)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Server response failed");

    // Call the function related to the selected game to properly unpack the data
    if((result=unpackGetBoardState(string, tokens, boardState)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to unpack board state");

    free(string);
    free(tokens);

    // Return success
    return ALL_GOOD;

}

// This function is used to send a message to your opponent during a game.
// You need to provide the message as a string. It should be less than 256 characters long.

ResultCode sendMessage(const char *message) {
    ResultCode result;
    // Check user's provided data
    if(strlen(message) >= MAX_MESSAGE_LENGTH)
        return printError(__FUNCTION__, PARAM_ERROR, "Message too long");

    // Parse data into json string
    char* data = (char *) malloc((MAX_MESSAGE_LENGTH + 50) * sizeof(char));
    if(data == NULL)
        return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, "");

    int dataLength = sprintf(data, "{ 'action': 'sendMessage', 'message': '%s' }", message);

    // Send data and check for success
    if((result=sendData(data, dataLength)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to send data");
    free(data);

    // Get server acknowledgement
    char* string;
    if((result=getServerResponse(&string, NULL, SERVER_ACKNOWLEDGEMENT_JSON_SIZE)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Server response failed");
    free(string);

    // Return success
    return ALL_GOOD;
}

// This function is used to display the game board during a game.
// It will print the colored board in the console.

ResultCode printBoard() {
    ResultCode result;
    // Parse data into json string
    char* data = "{ 'action': 'displayGame' }";

    // Send data and check for success
    if((result=sendData(data, strlen(data))) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to send data");

    // Get server acknowledgement
    char* string;
    jsmntok_t* tokens;
    if((result=getServerResponse(&string, &tokens, SERVER_ACKNOWLEDGEMENT_JSON_SIZE)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Server response failed");

    int blockLength = atoi(&string[tokens[4].start]) + 1;
    char* buffer = (char *) malloc(blockLength * sizeof(char));
    if(buffer == NULL)
        return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, "");

    // Fill buffer with 0
    memset(buffer, 0, blockLength * sizeof(char));

    // Read new incoming message on the socket wire
    if((result=readNByte(&buffer, blockLength - 1)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to read data");

    // Print the board
    printf("%s\n", buffer);

    free(buffer);
    free(string);
    free(tokens);

    // Return success
    return ALL_GOOD;
}


// This function is used to quit the currently running game.
ResultCode quitGame() {
    ResultCode result;
    // Parse data into json string
    char* data = "{ 'action': 'quitGame' }";

    // Send data and check for success
    if((result=sendData(data, strlen(data))) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Failed to send data");

    // Get server acknowledgement
    char* string;
    jsmntok_t* tokens;
    if((result=getServerResponse(&string, &tokens, SERVER_ACKNOWLEDGEMENT_JSON_SIZE)) != ALL_GOOD)
        return printError(__FUNCTION__, result, "Server response failed");
    free(string);
    free(tokens);

    // Close the socket !
    close(SOCKET);
    printDebugMessage(__FUNCTION__, DEBUG, "Connection closed");

    // Return success
    return ALL_GOOD;
}



/*

    Hidden functions

    Those are the functions used by the exposed functions to interact with the server
    You are not supposed to use them directly

*/

static ResultCode connectToSocket(const char *address, unsigned int port, int adrType) {
    int soc = socket(adrType, SOCK_STREAM, 0); // Use TCP socket
    if (soc < 0)
        return printError(__FUNCTION__, OTHER_ERROR, "Socket creation failed");

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = adrType;
    serv_addr.sin_port = htons(port);

    int res = inet_pton(adrType, address, &serv_addr.sin_addr);
    if (res <= 0)
        return printError(__FUNCTION__, PARAM_ERROR, "Invalid address / address not supported");

    int status = connect(soc, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    if (status < 0)
        return printError(__FUNCTION__, SERVER_ERROR, "Connection to server failed: %s [code = %d]", strerror(errno), errno);

    SOCKET = soc;
    printDebugMessage(__FUNCTION__, INTERN_DEBUG, "Socket created");
    return ALL_GOOD;
}

static ResultCode dnsSearch(const char *domain, char** ipaddress, int* adrType) {
    // Do a DNS search to resolve domain name
    struct addrinfo* dnsResult = NULL;
    int result = getaddrinfo(domain, 0, 0, &dnsResult);

    if(result != 0)
        return printError(__FUNCTION__, OTHER_ERROR, "DNS search failed");

    // Get IP address type
    *adrType = dnsResult->ai_addr->sa_family;

    int adrSize = *adrType == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN;
    *ipaddress = (char *) malloc(adrSize * sizeof(char));
    if(*ipaddress == NULL)
        return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, "");

    // Convert IP address to string
    if(dnsResult->ai_addr->sa_family == AF_INET) { // IPV4 address found
        struct sockaddr_in *p = (struct sockaddr_in *) dnsResult->ai_addr;
        inet_ntop(AF_INET, &p->sin_addr, *ipaddress, adrSize);
    } else if (dnsResult->ai_addr->sa_family == AF_INET6) { // IPV6 address found
        struct sockaddr_in6 *p = (struct sockaddr_in6 *) dnsResult->ai_addr;
        inet_ntop(AF_INET6, &p->sin6_addr, *ipaddress, adrSize);
    }

    freeaddrinfo(dnsResult);
    return ALL_GOOD;
}

// return ALL_GOOD or OTHER_ERROR
static ResultCode sendData(const char *data, unsigned int dataLength) {
    // check if the socket is open
    if (SOCKET < 0)
        return printError(__FUNCTION__, OTHER_ERROR, "The connection to the server is not yet established. Call `connectToCGS` before!");

    // Allocate data block for first message containing next message length
    char* dataBlock1 = (char *) malloc(FIRST_MSG_LENGTH * sizeof(char));
    if(dataBlock1 == NULL) return MEMORY_ALLOCATION_ERROR;

    // Fill string with spaces
    for(int i = 0; i < FIRST_MSG_LENGTH; i++)
        dataBlock1[i] = ' ';

    // Fill string with the length of the next message
    sprintf(dataBlock1, "%d", dataLength);

    // Send first message over the socket wire
    if(send(SOCKET, dataBlock1, FIRST_MSG_LENGTH, 0) == -1)
        return printError(__FUNCTION__, OTHER_ERROR, "Failed to send data");

    free(dataBlock1);

    // Send data over the socket wire
    if(send(SOCKET, data, dataLength, 0) == -1)
        return printError(__FUNCTION__, OTHER_ERROR, "Failed to send data");

    // Return success
    printDebugMessage(__FUNCTION__, INTERN_DEBUG, "Sent data: %s", data);
    return ALL_GOOD;
}

// tokens can be NULL if we don't care about the answer
static ResultCode getServerResponse(char **string, jsmntok_t **tokens, int nbMaxTokens) {
    int stringLength;
    ResultCode result;
    bool nullTokens = false;

    // Get data
    if((result=getData(string, &stringLength)) != ALL_GOOD)
        return result;
    printDebugMessage(__FUNCTION__, INTERN_DEBUG, "Server answered: %s", *string);

    // Instantiate json parser
    jsmn_parser parser;
    jsmn_init(&parser);

    // Parse json string
    if (tokens == NULL) {
        tokens = (jsmntok_t **) malloc(sizeof(jsmntok_t**));
        if(tokens == NULL)
            return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, "");
        nullTokens = true;
    }
    *tokens = (jsmntok_t *) malloc(nbMaxTokens * sizeof(jsmntok_t));
    if(*tokens == NULL)
        return printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, "");
    jsmn_parse(&parser, *string, stringLength, *tokens, nbMaxTokens);

    // Get state infos
    int state = getIntFromTokens(*string, "state", *tokens, nbMaxTokens);

    // Print error if needed
    if(!state) {
        // get the error
        char* error = getStringFromTokens(*string, "error", *tokens, nbMaxTokens);
        printError(__FUNCTION__, OTHER_ERROR, "Server responded with following error: %s\n", error);
        free(error);
        return OTHER_ERROR;
    }

    // dealloc if tokens was passed as NULL
    if (nullTokens) {
        free(*tokens);
        free(tokens);
    }

    // Return success
    return ALL_GOOD;
}

static ResultCode getData(char **string, int *stringLength) {
    // Allocate buffer to store data from read
    char buffer[FIRST_MSG_LENGTH] = { 0 };

    // Read incoming data on socket wire
    int res = read(SOCKET, buffer, FIRST_MSG_LENGTH - 1);
    if(res <= 0) return OTHER_ERROR; // Ensure it reads the correct amount of data

    // First message contain the length of the next one
    *stringLength = atoi(buffer) + 1;

    // Allocate buffer of 0 based on the next message length
    char* buffer2 = (char *) malloc(*stringLength * sizeof(char));
    if(buffer2 == NULL) return MEMORY_ALLOCATION_ERROR;

    // Fill buffer with 0
    memset(buffer2, 0, (*stringLength) * sizeof(char));

    // Read new incoming message on the socket wire
    res = readNByte(&buffer2, *stringLength - 1);

    // Check for error
    if(res != ALL_GOOD) return res;
    
    // Copy received data to **string param to effectively return the received string
    *string = (char *) malloc(*stringLength * sizeof(char));
    if(*string == NULL) return MEMORY_ALLOCATION_ERROR;

    strcpy(*string, buffer2);

    // Return success
    return ALL_GOOD;
}

static ResultCode readNByte(char **buffer, int nbByte) {
    int totalRead = 0;
    while (totalRead < nbByte) {
        int res2 = read(SOCKET, *buffer + totalRead, nbByte - totalRead);
        totalRead += res2;

        // Check for error
        if (res2 <= 0) return SERVER_ERROR;
    }

    return ALL_GOOD;
}

/*

    Utils

*/

static int getIntegerLength(int value) {
    int l = !value;
    while(value) { l++; value /= 10; }

    return l;
}

static int isValidIpAddress(const char *ipAddress) {
    struct sockaddr_in sa;

    // Assume IPV 4
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));

    if(result == 1) return AF_INET;
    else if(result == 0) { // Incorrect IP address family
        // Try IPV 6
        result = inet_pton(AF_INET6, ipAddress, &(sa.sin_addr));

        if(result <= 0) return result; // Invalid IP
        else return AF_INET6;
    } else return result; // Invalid IP address format (IPV 4 or IPV 6)
}

/*

    Debug tools

*/

ResultCode printError(const char* function, ResultCode code, const char* message, ...) {
    /* print error code */
    switch(code) {
        case PARAM_ERROR:
            printf("\x1b[1;31m[%s] Invalid parameters\x1b[0m\n", function);
            break;
        case SERVER_ERROR:
            printf("\x1b[1;31m[%s] Server error\x1b[0m\n", function);
            break;
        case MEMORY_ALLOCATION_ERROR:
            printf("\x1b[1;31m[%s] Memory allocation failed\x1b[0m\n", function);
            break;
        case OTHER_ERROR:
            printf("\x1b[1;31m[%s] Other error\x1b[0m\n", function);
            break;
        default:
            printf("\x1b[1;31m[%s] Unknown error code\x1b[0m\n", function);
            break;
    }
    /* and extra message if given */
    if (*message) {
        va_list args;
        va_start(args, message);
        printf("\x1b[1;31m  > ");
        vprintf(message, args);
        printf("\x1b[0m\n");
        va_end(args);
    }
    /* stop on error */
    if (DEBUG_LEVEL <= DEBUG)
        return code;
    exit(code);
}

void printDebugMessage(const char* function, unsigned int level, const char* message, ...) {
    const static char* levelString[] = {"\x1b[1;30m", "\x1b[1;30m", "\x1b[1;31m", "\x1b[1;32m", "\x1b[1;35m"};
    if(DEBUG_LEVEL>=level) {
        va_list args;
        va_start(args, message);
        printf("%s[%s] ", levelString[level], function);
        vprintf(message, args);
        printf("\x1b[0m\n");
        va_end(args);
    }
}

int getIntFromTokens(const char *string, const char* prop, const jsmntok_t *tokens, int nbMaxTokens) {
    int i = searchInTokens(string, prop, tokens, nbMaxTokens);
    if (i<nbMaxTokens) {
        char* st = (char *) malloc(tokens[i+1].end - tokens[i+1].start + 1);
        if (st == NULL)
            printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, NULL);

        // Fill st with 0
        memset(st, 0, tokens[i+1].end - tokens[i+1].start + 1);
        strncpy(st, string+tokens[i+1].start, tokens[i+1].end - tokens[i+1].start);

        char *stopped;
        int integer = (int) strtol(st, &stopped, 10);
        if (*stopped)
            printError(__FUNCTION__, SERVER_ERROR, "Cannot parse the server message, the value associated to `%s` is not an integer in message %s", prop, string);
        free(st);
        return integer;
    }
    return 0;

}

char* getStringFromTokens(const char *string, const char* prop, const jsmntok_t *tokens, int nbMaxTokens) {
    int i = searchInTokens(string, prop, tokens, nbMaxTokens);
    if (i<nbMaxTokens) {
        char* st = (char *) malloc(tokens[i+1].end - tokens[i+1].start + 1);
        if (st == NULL)
            printError(__FUNCTION__, MEMORY_ALLOCATION_ERROR, NULL);
        strncpy(st, string+tokens[i+1].start, tokens[i+1].end - tokens[i+1].start);
        return st;
    }
    else return "";
}

// get the index where a given property `prop` can be found. The associate value should be at the next index
int searchInTokens(const char *string, const char* prop, const jsmntok_t *tokens, int nbMaxTokens) {
    int i;
    // search for the token
    for (i=1; i<nbMaxTokens; i+=2) {
        if ((tokens[i].type == JSMN_STRING) && !strncmp(prop, string+tokens[i].start, tokens[i].end - tokens[i].start))
            break;
    }
    // if not found
    if (i>=nbMaxTokens) {
        printError(__FUNCTION__, SERVER_ERROR, "Cannot parse the server message, looking for string value for `%s` in message %s", prop, string);
    }
    return i;
}