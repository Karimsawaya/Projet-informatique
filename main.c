#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "ticketToRide.h"

#define PORT 15001
#define SERVER_ADDRESS "82.64.1.174"

GameSettings gameSettings;
GameData gameData;

void ConnectToServer() {
    int result = connectToCGS(SERVER_ADDRESS, PORT);
    if (result != ALL_GOOD) {
        printf("Connection failed (error code: %d)\n", result);
        exit(1);
    }
    printf("Successfully connected to the server!\n");
}

void SendGameName() {
    const char *playerName = "Karim";
    int result = sendName(playerName);
    if (result != ALL_GOOD) {
        printf("Error sending name (error code: %d)\n", result);
        exit(1);
    }
}

void SetGameSettings() {
    gameSettings = GameSettingsDefaults;
    gameData = GameDataDefaults;

    int result = sendGameSettings(gameSettings, &gameData);

    if (result != ALL_GOOD) {
        printf("Error sending game settings (error code: %d)\n", result);
        exit(1);
    }

    printf("Starter final = %d\n", gameData.starter); 
}

int main() {
    ConnectToServer();
    SendGameName();
    SetGameSettings();

    if (gameData.starter == 1) {
        printf("NOT YOUR TURN\n");
        exit(EXIT_FAILURE);
    }    

    if (gameData.starter == 2) {
        printf("C'est notre tour !\n");

        MoveData move;
        MoveResult resultMove;

        move.action = DRAW_OBJECTIVES;

        ResultCode code = sendMove(&move, &resultMove);
        if (code != ALL_GOOD) {
            printf("Erreur lors de l'envoi du move.\n");
            exit(1);
        }

        printf("[MoveResult] state = %d\n", resultMove.state);
        for (int i = 0; i < 3; i++) {
            printf("Objectif %d: de %u à %u pour %u points\n",
                   i + 1,
                   resultMove.objectives[i].from,
                   resultMove.objectives[i].to,
                   resultMove.objectives[i].score);
        }

        // Envoi du choix des objectifs
        MoveData choix;
        MoveResult resultChoix;

        choix.action = CHOOSE_OBJECTIVES;
        choix.chooseObjectives[0] = true;
        choix.chooseObjectives[1] = false;
        choix.chooseObjectives[2] = true;

        code = sendMove(&choix, &resultChoix);
        if (code != ALL_GOOD) {
            printf("Erreur lors de CHOOSE_OBJECTIVES.\n");
            exit(1);
        }

        printf("Choix des objectifs effectué. Move state = %d\n", resultChoix.state);

        // Libérer les messages si alloués
        free(resultMove.message);
        free(resultMove.opponentMessage);
        free(resultChoix.message);
        free(resultChoix.opponentMessage);
    }

    return 0;
}


//1) Programme capable de jouer manuellement  -- > scanf

// 2) Structure de données 

// 3) Mettre a jour les donnees --> afficher les donnees 

// 4) Un bot  
// -- Random player deja , des qu il a suffisament de carte pour poser une route il le fait !  Il doit battre randomplayer
// 

//2 NOTRE TOUR
//1 BOT TOUR