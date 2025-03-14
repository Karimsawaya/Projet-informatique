#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "ticketToRide.h"

#define PORT 15001
#define SERVER_ADDRESS "82.64.1.174"

void cleanupMoveResult(MoveResult *moveResult) {
    if (moveResult->opponentMessage) {
        free(moveResult->opponentMessage);
        moveResult->opponentMessage = NULL;
    }
    if (moveResult->message) {
        free(moveResult->message);
        moveResult->message = NULL;
    }
}

void cleanupGameData(GameData *gameData) {
    if (gameData->gameName) {
        free(gameData->gameName);
        gameData->gameName = NULL;
    }
    if (gameData->trackData) {
        free(gameData->trackData);
        gameData->trackData = NULL;
    }
}

void playTurn() {
    MoveData myMove;
    MoveResult myMoveResult;
    ResultCode result;


    // Display board state
    result = printBoard();
    if (result != ALL_GOOD) {
        printf("Error displaying board (error code: %d)\n", result);
    }

    // Get board state (visible cards)
    BoardState boardState;
    result = getBoardState(&boardState);
    if (result == ALL_GOOD) {
        printf("Visible cards: \n");
        for (int i = 0; i < 5; i++) {
            printf("%d ", boardState.card[i]);
        }
        printf("\n");
    } else {
        printf("Error getting board state (error code: %d)\n", result);
    }

    printf("Choose an action:\n\n");
    printf("1 - Claim a route\n");
    printf("2 - Draw a blind card\n");
    printf("3 - Draw a visible card\n");
    printf("4 - Draw objectives\n");
    printf("5 - Choose objectives\n");
    printf("6 - Send a message to opponent\n");
    printf("0 - Quit\n");

    int choice;
    scanf("%d", &choice);

    if (choice == 0) {
        exit(0);
    }

    switch (choice) {
        case 1:
            printf("Enter route (from city ID, to city ID, color ID, locomotives): ");
            scanf("%d %d %d %d", &myMove.claimRoute.from, &myMove.claimRoute.to,
                 (int*)&myMove.claimRoute.color, &myMove.claimRoute.nbLocomotives);
            myMove.action = CLAIM_ROUTE;
            break;
        case 2:
            myMove.action = DRAW_BLIND_CARD;
            break;
        case 3:
            printf("Enter card color (1-9): ");
            scanf("%d", (int*)&myMove.drawCard);
            myMove.action = DRAW_CARD;
            break;
        case 4:
            myMove.action = DRAW_OBJECTIVES;
            break;
        case 5:
            printf("Choose objectives (1 to keep, 0 to discard) [3 values]: ");
            scanf("%d %d %d", (int*)&myMove.chooseObjectives[0],
                 (int*)&myMove.chooseObjectives[1], (int*)&myMove.chooseObjectives[2]);
            myMove.action = CHOOSE_OBJECTIVES;
            break;
        case 6: {
            char message[256];
            printf("Enter message to opponent: ");
            scanf(" %[^\n]", message);
            ResultCode result = sendMessage(message);
            if (result != ALL_GOOD) {
                printf("Error sending message (error code: %d)\n", result);
            } else {
                printf("Message sent!\n");
            }
            return;
        }
        default:
            printf("Invalid choice!\n");
            return;
    }

    // Send move to server
    result = sendMove(&myMove, &myMoveResult);
    if (result != ALL_GOOD) {
        printf("Error sending move (error code: %d)\n", result);
        return;
    }

    cleanupMoveResult(&myMoveResult);
}
void opponentTurn() {
    MoveData opponentMove;
    MoveResult opponentResult;
    ResultCode result;

    printf("\n=== OPPONENT'S TURN ===\n");
    printf("Waiting for opponent's move...\n");

    result = getMove(&opponentMove, &opponentResult);
    if (result != ALL_GOOD) {
        printf("Error getting opponent move (error code: %d)\n", result);
        return;
    }
    else{
        printf("Received opponents move");
    }
}


int main() {
    ResultCode result;

    printf("Connecting to server...\n");
    result = connectToCGS(SERVER_ADDRESS, PORT);
    if (result != ALL_GOOD) {
        printf("Connection failed (error code: %d)\n", result);
        return 1;
    }

    printf("Successfully connected to the server!\n");

    const char *playerName = "Karim";
    printf("Player name: %s\n", playerName);
    result = sendName(playerName);
    if (result != ALL_GOOD) {
        printf("Error sending name (error code: %d)\n", result);
        return 1;
    }

    GameSettings gameSettings = GameSettingsDefaults;
    GameData gameData = GameDataDefaults;

    result = sendGameSettings(gameSettings, &gameData);
    if (result != ALL_GOOD) {
        printf("Error sending game settings (error code: %d)\n", result);
        return 1;
    }

    printf("Game name: %s\n", gameData.gameName);
    printf("Game seed: %d\n", gameData.gameSeed);
    printf("Starter: %d (1 means you start)\n", gameData.starter);
    printf("Number of cities: %d\n", gameData.nbCities);
    printf("Number of tracks: %d\n", gameData.nbTracks);

    printf("Your initial cards: ");
    for (int i = 0; i < 4; i++) {
        printf("%d ", gameData.cards[i]);
    }
    printf("\n");

   if(gameData.starter == 1) {
    playTurn();
   }
   else {
    opponentTurn();
   }
    


        
    return 0;
}
