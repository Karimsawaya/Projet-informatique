#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "ticketToRide.h"
#include "clientAPI.h"
#include <string.h>
#include <unistd.h>

#define SERVER_ADDRESS "82.29.170.160"
#define PORT 15001
#define MAX_CITIES 35
#define INFINITY 1000000  // Valeur très grande simulant l'infini

int cheminVersObjectif[MAX_CITIES];
int cheminLen = 0;



typedef struct {
    int dist[MAX_CITIES];     // La distance minimale pour atteindre chaque ville
    int prev[MAX_CITIES];     // Pour chaque ville : la ville précédente dans le chemin optimal
    bool visited[MAX_CITIES]; // Villes déjà traitées (pas utile à toi après calcul)
} DijkstraResult;


DijkstraResult dijkstra(int from, int nbCities);

// Structure d'une route entre deux villes
typedef struct {
    int from;
    int to;
    int length;
    CardColor color;
    bool taken;
} Route;


typedef struct {
    int nbWagons;
    int nbCartes;
    int nbObjectifs;
    int cartes[10];
    Objective objectifs[20];
} Joueur;

typedef struct {
    int joueurActif;
    int monId;
    bool phaseInitialeTerminee;
    CardColor cartesVisibles[5];
    Joueur joueurs[2];
    Route* routes[100][100]; // carte [ville1][ville2]
    int toursJoues;
} Partie;


Partie partie;

void safeFree(char** ptr) {
    if (*ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

ResultCode Connexion(const char* nomBot) {
    ResultCode res = connectToCGS(SERVER_ADDRESS, PORT, nomBot);
    printf(res == ALL_GOOD ? "Connexion reussie !\n" : "Erreur connexion : 0x%x\n", res);
    return res;
}


ResultCode SendParameters(GameData* gameData) {
    const char* settings = "TRAINING NICE_BOT";
    ResultCode res = sendGameSettings(settings, gameData);

    if (res == ALL_GOOD) {
        printf(" Partie : %s | Villes : %d | Routes : %d | Seed : %d\n",
               gameData->gameName, gameData->nbCities, gameData->nbTracks, gameData->gameSeed);

        partie.monId = 0;
        partie.joueurActif = (gameData->starter == 0) ? 0 : 1;
        partie.phaseInitialeTerminee = false;

        for (int p = 0; p < 2; p++) {
            partie.joueurs[p].nbWagons = 45;
            partie.joueurs[p].nbCartes = 0;
            partie.joueurs[p].nbObjectifs = 0;
            for (int c = 0; c < 10; c++) {
                partie.joueurs[p].cartes[c] = 0;
            }
        }

        for (int i = 0; i < MAX_CITIES; i++) {
            for (int j = 0; j < MAX_CITIES; j++) {
                partie.routes[i][j] = NULL;
            }
        }

        for (int i = 0; i < gameData->nbTracks; i++) {
            int from   = gameData->trackData[i * 5 + 0];
            int to     = gameData->trackData[i * 5 + 1];
            int length = gameData->trackData[i * 5 + 2];
            CardColor color1 = (CardColor)gameData->trackData[i * 5 + 3];

            if (color1 != NONE) {
                Route* r1 = malloc(sizeof(Route));
                r1->from = from;
                r1->to = to;
                r1->length = length;
                r1->color = color1;
                r1->taken = false;
                partie.routes[from][to] = r1;
                partie.routes[to][from] = r1;
            }

            
        }

        printf("Cartes initiales reçues :\n");
        for (int i = 0; i < 4; i++) {
            CardColor couleur = gameData->cards[i];
            printf("  Carte %d: couleur %d\n", i + 1, couleur);
            if (couleur >= 0 && couleur < 10) {
                partie.joueurs[partie.monId].cartes[couleur]++;
                partie.joueurs[partie.monId].nbCartes++;
            }
        }
        printf("\n");

        free(gameData->gameName);
        free(gameData->trackData);
    } else {
        printf("Erreur lors de l'envoi des paramètres : 0x%x\n", res);
    }

    return res;
}


void afficherCartesEnMain() {
    Joueur* moi = &partie.joueurs[partie.monId];
    printf("\n\n=== Mes cartes en main (%d cartes) ===\n\n", moi->nbCartes);
    const char* couleurs[] = {"NONE", "PURPLE", "WHITE", "BLUE", "YELLOW", "ORANGE", "BLACK", "RED", "GREEN", "LOCOMOTIVE"};
    
    for (int i = 0; i < 10; i++) {
        if (moi->cartes[i] > 0) {
            printf("  %s : %d\n", couleurs[i], moi->cartes[i]);
        }
    }
    printf("\n\nWagons restants : %d\n\n", moi->nbWagons);
    
    printf("\n\n=== Mes objectifs (%d) ===\n\n", moi->nbObjectifs);
    for (int i = 0; i < moi->nbObjectifs; i++) {
        printf("  ");
        printCity(moi->objectifs[i].from);
        printf(" -> ");
        printCity(moi->objectifs[i].to);
        printf(" (%d points)\n", moi->objectifs[i].score);
    }
}


ResultCode ClaimRoute(int from, int to, CardColor couleur, int nbLocos) {
    Route* route = partie.routes[from][to];
    Joueur* moi = &partie.joueurs[partie.monId];

    //  Vérification route existante
    if (!route || route->taken) {
        printf(" Route inexistante ou déjà prise : %d → %d\n", from, to);
        return 99;
    }

    int longueur = route->length;
    int wagons = moi -> nbWagons;
    int cartesCouleur = moi->cartes[couleur];
    int cartesLocos = moi->cartes[LOCOMOTIVE];
    int cartesTotales = cartesCouleur + cartesLocos;

    //  Vérifications avant envoi au serveur
    if (wagons < longueur) {
        printf(" Pas assez de wagons : %d requis, %d disponibles\n", longueur, moi->nbWagons);
        return 99;
    }

    if (cartesTotales < longueur) {
        printf(" Pas assez de cartes (couleur + locos) : %d requis, %d disponibles\n", longueur, cartesTotales);
        return 99;
    }

    int cartesClassiques = longueur - nbLocos;
    
    if (cartesCouleur < cartesClassiques) {
        printf(" Pas assez de cartes %d pour compenser %d classiques\n", cartesCouleur, cartesClassiques);
        return 99;
    }

    if (cartesLocos < nbLocos) {
        printf(" Pas assez de locomotives : %d requis, %d disponibles\n", nbLocos, cartesLocos);
        return 99;
    }

    if (moi->nbCartes < longueur) {
        printf(" Pas assez de cartes totales (bug possible si >50)\n");
        return 99;
    }

    // Construction du mouvement
    MoveData move = { .action = CLAIM_ROUTE };
    move.claimRoute.from = from;
    move.claimRoute.to = to;
    move.claimRoute.color = couleur;
    move.claimRoute.nbLocomotives = nbLocos;

    MoveResult result = {0};
    ResultCode res = sendMove(&move, &result);

    if (res != ALL_GOOD) {
        printf(" Échec prise de route (serveur) : code 0x%x\n", res);
        if (result.message) {
            printf(" Message du serveur : %s\n", result.message);
        }
        safeFree(&result.message);
        safeFree(&result.opponentMessage);
        return res;
    }

    //  Réponse serveur positive → mise à jour locale
    moi->cartes[couleur] -= cartesClassiques;
    moi->cartes[LOCOMOTIVE] -= nbLocos;
    moi->nbCartes -= longueur;
    moi->nbWagons -= longueur;

    route->taken = true;
    partie.routes[to][from]->taken = true;

    printf(" Route prise : ");
    printCity(from); printf(" → "); printCity(to); printf("\n");

    partie.joueurActif = 1 - partie.joueurActif;

    safeFree(&result.message);
    safeFree(&result.opponentMessage);
    return res;
}


ResultCode DrawObjectives(Objective* buffer) {
    MoveData move = { .action = DRAW_OBJECTIVES };
    MoveResult result = {0};

    ResultCode res = sendMove(&move, &result);
    if (res != ALL_GOOD) {
        printf("Erreur DRAW_OBJECTIVES : 0x%x\n", res);
        return res;
    }

    printf("Objectifs reçus :\n");
    for (int i = 0; i < 3; i++) {
        buffer[i] = result.objectives[i];
        printf("  Objectif %d : ", i + 1);
        printCity(buffer[i].from);
        printf(" -> ");
        printCity(buffer[i].to);
        printf(" (%d points)\n", buffer[i].score);
    }

    // Vérifier si on doit rejouer
    if (result.replay) {
        printf("\n\n-> On doit (CHOOSE_OBJECTIVES)\n\n");
    }

    //  PAS de changement de joueur actif ici !

    safeFree(&result.message);
    safeFree(&result.opponentMessage);
    return ALL_GOOD;
}

ResultCode ChooseObjectives(Objective* objectifsReçus, bool* choix) {
    MoveData move = { .action = CHOOSE_OBJECTIVES };

    move.chooseObjectives[0] = choix[0];
    move.chooseObjectives[1] = choix[1];
    move.chooseObjectives[2] = choix[2];

    MoveResult result = {0};
    ResultCode res = sendMove(&move, &result);
    if (res != ALL_GOOD) {
        printf("Erreur CHOOSE_OBJECTIVES : 0x%x\n", res);
        return res;
    }

    Joueur* moi = &partie.joueurs[partie.monId];
    
    int indexChoisi = moi->nbObjectifs;  // Ajouter APRÈS les existants
    
    printf("Objectifs choisis :\n");
    for (int i = 0; i < 3; i++) {
        if (choix[i]) {
            moi->objectifs[indexChoisi] = objectifsReçus[i];
            printf("  ");
            printCity(moi->objectifs[indexChoisi].from);
            printf(" -> ");
            printCity(moi->objectifs[indexChoisi].to);
            printf(" (%d points)\n", moi->objectifs[indexChoisi].score);
            indexChoisi++;
            moi->nbObjectifs++;
        }
    }

    partie.joueurActif = 1 - partie.joueurActif;
    printf("[CHANGEMENT] Fin de nos objectifs, joueur actif: %d\n", partie.joueurActif);

    safeFree(&result.message);
    safeFree(&result.opponentMessage);
    return ALL_GOOD;
}


ResultCode GetMove() {
    MoveData move = {0};
    MoveResult result = {0};

    ResultCode res = getMove(&move, &result);
    if (res != ALL_GOOD) {
        printf("Erreur getMove : 0x%x\n", res);
        return res;
    }

    printf(" \n\nAdversaire a joué : \n\n");

    switch (move.action) {
        case DRAW_OBJECTIVES:
            printf("DRAW_OBJECTIVES\n");
            break;

        case CHOOSE_OBJECTIVES:
            printf("CHOOSE_OBJECTIVES (");
            for (int i = 0; i < 3; i++) {
                if (move.chooseObjectives[i]) {
                    printf("obj%d ", i + 1);
                }
            }
            printf(")\n");

            // CHANGEMENT JOUEUR ACTIF : après choix d'objectifs
            partie.joueurActif = 1 - partie.joueurActif;
            printf(" [CHANGEMENT] Fin objectifs adversaire, joueur actif: %d\n", partie.joueurActif);
            break;

        case DRAW_BLIND_CARD:
            printf("\nDRAW_BLIND_CARD\n");
            if (result.replay) {
                printf(" (peut rejouer)");
            }
            printf("\n");
            partie.joueurs[1 - partie.monId].nbCartes++;
            break;

        case DRAW_CARD:
            printf("DRAW_CARD (couleur %d)", move.drawCard);
            if (result.replay) {
                printf(" (peut rejouer)");
            }
            printf("\n");
            partie.joueurs[1 - partie.monId].nbCartes++;
            break;

        case CLAIM_ROUTE:
            printf("CLAIM_ROUTE de ");
            printCity(move.claimRoute.from);
            printf(" à ");
            printCity(move.claimRoute.to);
            printf(" (couleur %d, %d locomotives)\n",
                   move.claimRoute.color, move.claimRoute.nbLocomotives);

            // Mettre à jour les wagons de l'adversaire
            if (partie.routes[move.claimRoute.from][move.claimRoute.to]) {
                int longueur = partie.routes[move.claimRoute.from][move.claimRoute.to]->length;
                partie.joueurs[1 - partie.monId].nbWagons -= longueur;
                if (partie.joueurs[1 - partie.monId].nbWagons < 0)
                    partie.joueurs[1 - partie.monId].nbWagons = 0; // éviter négatif
                printf(" Adversaire a utilisé %d wagons, il lui en reste : %d\n",
                       longueur, partie.joueurs[1 - partie.monId].nbWagons);

                // Marquer la route comme prise
                partie.routes[move.claimRoute.from][move.claimRoute.to]->taken = true;
                partie.routes[move.claimRoute.to][move.claimRoute.from]->taken = true;
            }
            break;
    }

    // Gestion du joueur actif selon la phase et rejouabilité
    if (!result.replay && partie.phaseInitialeTerminee && move.action != CHOOSE_OBJECTIVES) {
        partie.joueurActif = 1 - partie.joueurActif;
        printf(" [CHANGEMENT] Fin tour adversaire, joueur actif: %d\n\n", partie.joueurActif);
    } else if (!result.replay && !partie.phaseInitialeTerminee && move.action != CHOOSE_OBJECTIVES) {
        printf("-> Fin action adversaire (phase initiale)\n");
    } else if (move.action != CHOOSE_OBJECTIVES) {
        printf("-> L'adversaire doit rejouer\n");
    }

    // Affichage des éventuels messages serveur/adversaire
    if (result.opponentMessage) {
        printf(" Message de l'adversaire : %s\n", result.opponentMessage);
    }
    if (result.message) {
        printf(" Message du serveur : %s\n", result.message);
    }

    safeFree(&result.message);
    safeFree(&result.opponentMessage);
    return ALL_GOOD;
}


ResultCode DrawCard(CardColor couleur) {
    MoveData move;
    MoveResult result;

    move.action = DRAW_CARD;
    move.drawCard = couleur;

    printf("[Action] Tirer carte visible de couleur : %d\n", couleur);

    ResultCode res = sendMove(&move, &result);
    if (res != ALL_GOOD) {
        printf(" Erreur lors de l’envoi de DRAW_CARD : code %d\n", res);
        return res;
    }

    if (result.state == DRAW_CARD) {
        partie.joueurs[partie.monId].cartes[result.card]++;
        partie.joueurs[partie.monId].nbCartes++;
        printf("[Résultat] Carte reçue : couleur %d\n", result.card);
    } else {
        printf(" Résultat inattendu après DRAW_CARD : %d\n", result.state);
    }

    if (result.message) free(result.message);
    if (result.opponentMessage) free(result.opponentMessage);

    return res;
}



// Fonction pour jouer deux cartes de la pioche
ResultCode DrawTwoBlindCards() {
    printf(" \nAction : Tirer 2 cartes de la pioche\n");
    
    // Première carte
    MoveData move1 = { .action = DRAW_BLIND_CARD };
    MoveResult result1 = {0};
    
    ResultCode res = sendMove(&move1, &result1);
    if (res != ALL_GOOD) {
        printf("Erreur DRAW_BLIND_CARD 1 : 0x%x\n", res);
        return res;
    }
    
    printf("\nCarte piochée 1 : couleur %d\n\n", result1.card);
    
    // Ajouter la carte à notre main avec vérification
    Joueur* moi = &partie.joueurs[partie.monId];
    if (result1.card >= 0 && result1.card < 10) {
        moi->cartes[result1.card]++;
        moi->nbCartes++;
    } else {
        printf("Attention: carte invalide reçue (%d)\n", result1.card);
    }
    
    safeFree(&result1.message);
    safeFree(&result1.opponentMessage);
    
    // Deuxième carte (si on peut rejouer)
    if (result1.replay) {
        MoveData move2 = { .action = DRAW_BLIND_CARD };
        MoveResult result2 = {0};
        
        res = sendMove(&move2, &result2);
        if (res != ALL_GOOD) {
            printf("Erreur DRAW_BLIND_CARD 2 : 0x%x\n", res);
            return res;
        }
        
        printf("Carte piochée 2 : couleur %d\n", result2.card);
        
        // Ajouter la carte à notre main avec vérification
        if (result2.card >= 0 && result2.card < 10) {
            moi->cartes[result2.card]++;
            moi->nbCartes++;
        } else {
            printf("Attention: carte invalide reçue (%d)\n", result2.card);
        }
        
        safeFree(&result2.message);
        safeFree(&result2.opponentMessage);
    }
    
    //  CHANGEMENT JOUEUR ACTIF #4: Après notre tour de cartes
    partie.joueurActif = 1 - partie.joueurActif;
    printf(" [CHANGEMENT] Fin de notre tour cartes, joueur actif: %d\n", partie.joueurActif);
    
    return ALL_GOOD;
}


// Version alternative avec stratégie intelligente
void gererPhaseInitiale() {
    printf("\n=== PHASE INITIALE : OBJECTIFS ===\n");
    printf(" Mon ID: %d, Joueur actif: %d\n\n", partie.monId, partie.joueurActif);
    
    Objective objectifs[3];

    // Si c'est notre tour
    if (partie.monId == partie.joueurActif) {
        printf(" Nous commençons la partie\n");

        // Notre phase d'objectifs
        printf("\n--- Notre phase d'objectifs ---\n");
        if (DrawObjectives(objectifs) != ALL_GOOD) return;
        
        bool choix[3] = {true, true, true};  // Choisir les 2 premiers
        if (ChooseObjectives(objectifs, choix) != ALL_GOOD) return;

        printf("\n--- Phase d'objectifs de l'adversaire ---\n");
        if (GetMove() != ALL_GOOD) return; // DRAW_OBJECTIVES
        if (GetMove() != ALL_GOOD) return; // CHOOSE_OBJECTIVES
    }
    else {
        printf(" L'adversaire commence la partie\n");

        printf("\n--- Phase d'objectifs de l'adversaire ---\n");
        if (GetMove() != ALL_GOOD) return; // DRAW_OBJECTIVES
        if (GetMove() != ALL_GOOD) return; // CHOOSE_OBJECTIVES

        printf("\n--- Notre phase d'objectifs ---\n");
        if (DrawObjectives(objectifs) != ALL_GOOD) return;
        
        bool choix[3] = {true, true, true};  // Choisir les 2 premiers
        if (ChooseObjectives(objectifs, choix) != ALL_GOOD) return;
    }

    partie.phaseInitialeTerminee = true;
    printf("\nPhase initiale terminée ! Début du jeu normal.\n");
    afficherCartesEnMain();
}


void boucleTestDijkstra(int from, int to) {
    printf("\n=== TEST DE DIJKSTRA ===\n");
    printf("Source : ");
    printCity(from);
    printf(" | Destination : ");
    printCity(to);
    printf("\n");

    DijkstraResult result = dijkstra(from, MAX_CITIES);

    // Afficher tableau dist[]
    printf("\n--- Tableau des distances minimales ---\n");
    for (int i = 0; i < MAX_CITIES; i++) {
        printCity(i);
        printf(" : %d\n", result.dist[i]);
    }

    // Afficher tableau prev[]
    printf("\n--- Tableau des prédécesseurs ---\n");
    for (int i = 0; i < MAX_CITIES; i++) {
        printCity(i);
        printf(" ← ");
        if (result.prev[i] == -1) {
            printf("N/A\n");
        } else {
            printCity(result.prev[i]);
            printf("\n");
        }
    }

    // Afficher le chemin optimal reconstitué
    printf("\n--- Chemin optimal ---\n");
    if (result.dist[to] == INFINITY) {
        printf("Aucun chemin disponible.\n");
        return;
    }

    int path[MAX_CITIES];
    int len = 0;
    int current = to;
    while (current != -1) {
        path[len++] = current;
        current = result.prev[current];
    }

    printf("Chemin : ");
    for (int i = len - 1; i >= 0; i--) {
        printCity(path[i]);
        if (i > 0) printf(" -> ");
    }
    printf(" (longueur totale : %d)\n", result.dist[to]);
}



DijkstraResult dijkstra(int from, int nbCities) {
    DijkstraResult result;
    
    for (int i = 0; i < nbCities; i++) {
        result.dist[i] = INFINITY;
        result.prev[i] = -1;
        result.visited[i] = false;
    }
    result.dist[from] = 0;

    for (int count = 0; count < nbCities; count++) {
        int minDist = INFINITY;
        int u = -1;

        // Trouver le sommet u avec la plus petite distance non visitée
        for (int i = 0; i < nbCities; i++) {
            if (!result.visited[i] && result.dist[i] < minDist) {
                minDist = result.dist[i];
                u = i;
            }
        }

        if (u == -1) break;  // Tous les sommets accessibles ont été visités
        result.visited[u] = true;

        // Mettre à jour les distances de ses voisins
        for (int v = 0; v < nbCities; v++) {
            Route* r = partie.routes[u][v];
            if (r && !r->taken) {
                int alt = result.dist[u] + r->length;
                if (alt < result.dist[v]) {
                    result.dist[v] = alt;
                    result.prev[v] = u;
                }
            }
        }
    }

    return result;
}

int ObjMAX(Joueur* joueur) {
    if (joueur->nbObjectifs <= 0) {
        printf("Aucun objectif en main.\n");
        return -1;
    }

    int maxPoints = joueur->objectifs[0].score;  // On initialise avec le premier score réel
    int indexMax = 0;

    printf("\n[DEBUG] Liste des objectifs du joueur :\n");

    for (int i = 0; i < joueur->nbObjectifs; i++) {
        int score = joueur->objectifs[i].score;
        int from = joueur->objectifs[i].from;
        int to = joueur->objectifs[i].to;

        printf("  Objectif[%d] : ", i);
        printCity(from);
        printf(" -> ");
        printCity(to);
        printf(" | Score : %d\n", score);

        if (score > maxPoints) {
            maxPoints = score;
            indexMax = i;
        }
    }

    printf("[DEBUG] Objectif avec le plus haut score : index %d (score %d)\n", indexMax, maxPoints);
    return indexMax;
}


void CheminObjMAX() {
    Joueur* moi = &partie.joueurs[partie.monId];
    int indexObjectif = ObjMAX(moi);

    if (indexObjectif == -1 || moi->nbObjectifs == 0) {
        printf("Aucun objectif trouvé.\n");
        cheminLen = 0;
        return;
    }

    int from = moi->objectifs[indexObjectif].from;
    int to = moi->objectifs[indexObjectif].to;

    printf("\n=== PLANIFICATION : Objectif à haut score ===\n");
    printf("Objectif sélectionné : ");
    printCity(from);
    printf(" -> ");
    printCity(to);
    printf(" (%d points)\n", moi->objectifs[indexObjectif].score);

    DijkstraResult result = dijkstra(from, MAX_CITIES);

    if (result.dist[to] == INFINITY) {
        printf("Aucun chemin disponible.\n");
        cheminLen = 0;
        return;
    }

    int current = to;
    cheminLen = 0;
    while (current != -1) {
        cheminVersObjectif[cheminLen++] = current;
        current = result.prev[current];
    }

    printf("Chemin : ");
    for (int i = cheminLen - 1; i >= 0; i--) {
        printCity(cheminVersObjectif[i]);
        if (i > 0) printf(" -> ");
    }
    printf("\n");
}



void jouerTourVersObjectif() {
    Joueur* moi = &partie.joueurs[partie.monId];

    // Étape 1 : Générer un chemin si inexistant
    if (cheminLen == 0) {
        if (moi->nbObjectifs > 0) {
            CheminObjMAX();  // Version améliorée
        } else {
            // Plus d'objectifs, on en pioche de nouveaux
            Objective nouveaux[3];
            if (DrawObjectives(nouveaux) != ALL_GOOD) return;

            bool choix[3] = {true, true, true};  // On les garde tous
            if (ChooseObjectives(nouveaux, choix) != ALL_GOOD) return;

            CheminObjMAX();  // Replanification
        }
    }

    // Étape 2 : Si toujours pas de chemin valide
    if (cheminLen <= 1) {
        printf(" Aucun chemin disponible pour prise de route.\n");

        if (moi->nbCartes > 22) {  // Seuil encore plus agressif
            printf(" Trop de cartes (%d), recherche d'une route jouable au lieu de piocher.\n", moi->nbCartes);
            bool aJoue = false;

            for (int i = 0; i < MAX_CITIES && !aJoue; i++) {
                for (int j = 0; j < MAX_CITIES && !aJoue; j++) {
                    Route* r = partie.routes[i][j];
                    if (r && !r->taken && moi->nbWagons >= r->length &&
                        (moi->cartes[r->color]) >= r->length) {
                        int locos = (moi->cartes[r->color] >= r->length) ? 0 : r->length - moi->cartes[r->color];
                        if (ClaimRoute(i, j, r->color, locos) == ALL_GOOD) {
                            printf(" Route libre prise (%d → %d)\n", i, j);
                            aJoue = true;
                        }
                    }
                }
            }

            if (!aJoue) {
                printf(" Aucune route jouable malgré trop de cartes. On passe le tour.\n");
                return;
            }
            return;
        }

        // Pioche selon les besoins
        if (moi->nbCartes < 38) {  // Pioche plus activement
            DrawTwoBlindCards();
        } else {
            printf(" Assez de cartes, on passe ce tour.\n");
        }
        return;
    }

    // Étape 3 : Tentative de prise de la prochaine route du chemin
    int from = cheminVersObjectif[cheminLen - 1];
    int to   = cheminVersObjectif[cheminLen - 2];
    Route* route = partie.routes[from][to];

    // Route déjà prise → on réduit le chemin et recommence
    if (!route || route->taken) {
        printf(" Route (%d -> %d) inexistante ou déjà prise.\n", from, to);
        cheminLen--;
        jouerTourVersObjectif();
        return;
    }

    int longueur = route->length;
    CardColor couleur = route->color;
    int nbCouleur = moi->cartes[couleur];
    int nbLocos = moi->cartes[LOCOMOTIVE];
    int totalCartes = nbCouleur + nbLocos;

    if (moi->nbWagons < longueur) {
        printf(" Pas assez de wagons pour [%d -> %d]\n", from, to);
        cheminLen--;
        return;
    }

    // Gestion route LOCOMOTIVE
    if (couleur == LOCOMOTIVE) {
        if (nbLocos < longueur) {
            printf(" Pas assez de LOCOMOTIVES [%d -> %d].\n", from, to);

            if (moi->nbCartes > 35) {  // Plus agressif
                printf(" Trop de cartes (%d), tentative de route alternative.\n", moi->nbCartes);
                bool aJoue = false;
                for (int i = 0; i < MAX_CITIES && !aJoue; i++) {
                    for (int j = 0; j < MAX_CITIES && !aJoue; j++) {
                        Route* r = partie.routes[i][j];
                        if (r && !r->taken && moi->nbWagons >= r->length &&
                            (moi->cartes[r->color]) >= r->length) {
                            int locos = (moi->cartes[r->color] >= r->length) ? 0 : r->length - moi->cartes[r->color];
                            if (ClaimRoute(i, j, r->color, locos) == ALL_GOOD) {
                                printf(" Route libre prise (%d → %d)\n", i, j);
                                aJoue = true;
                            }
                        }
                    }
                }

                if (!aJoue) {
                    printf(" Aucune route jouable. Fin de tour.\n");
                }
                return;
            }

            DrawTwoBlindCards();
            return;
        }

        if (ClaimRoute(from, to, couleur, longueur) != ALL_GOOD) {
            printf(" Échec prise de route LOCOMOTIVE [%d -> %d]\n", from, to);
            DrawTwoBlindCards();
            return;
        }
    } else {
        // Gestion route normale
        if (totalCartes < longueur) {
            printf(" Pas assez de cartes pour [%d -> %d].\n", from, to);

            if (moi->nbCartes > 35) {  // Plus agressif
                printf(" Trop de cartes (%d), tentative de route alternative.\n", moi->nbCartes);
                bool aJoue = false;
                for (int i = 0; i < MAX_CITIES && !aJoue; i++) {
                    for (int j = 0; j < MAX_CITIES && !aJoue; j++) {
                        Route* r = partie.routes[i][j];
                        if (r && !r->taken && moi->nbWagons >= r->length &&
                            (moi->cartes[r->color]) >= r->length) {
                            int locos = (moi->cartes[r->color] >= r->length) ? 0 : r->length - moi->cartes[r->color];
                            if (ClaimRoute(i, j, r->color, locos) == ALL_GOOD) {
                                printf(" Route libre prise (%d → %d)\n", i, j);
                                aJoue = true;
                            }
                        }
                    }
                }

                if (!aJoue) {
                    printf(" Aucune route jouable. Fin de tour.\n");
                }
                return;
            }

            DrawTwoBlindCards();
            return;
        }

        int locosAUtiliser = (nbCouleur >= longueur) ? 0 : longueur - nbCouleur;
        if (ClaimRoute(from, to, couleur, locosAUtiliser) != ALL_GOOD) {
            printf(" Échec prise de route [%d -> %d].\n", from, to);
            DrawTwoBlindCards();
            return;
        }
    }

    // Route bien prise → on avance dans le chemin
    cheminLen--;

    if (cheminLen <= 1) {
        printf(" Étape d'objectif terminée ! Recherche du prochain objectif optimal...\n");
        CheminObjMAX();  // Cherche le PROCHAIN objectif optimal
    }
}


void afficherRoutes() {
    printf("\n=== ROUTES DISPONIBLES SUR LE PLATEAU ===\n");

    for (int i = 0; i < MAX_CITIES; i++) {
        for (int j = 0; j < MAX_CITIES; j++) {
            Route* r = partie.routes[i][j];
            if (r && i < j) { // éviter les doublons
                printf("[ID %2d] ", r->from);
                printCity(r->from);
                printf(" -> ");
                printf("[ID %2d] ", r->to);
                printCity(r->to);
                printf(" | Longueur: %d | Couleur: %d | Prise: %s\n",
                       r->length,
                       r->color,
                       r->taken ? "OUI" : "NON");
            }
        }
    }

    printf("=== FIN DE L'AFFICHAGE ===\n");
}


void boucleDeJeuPrincipale() {
    while (true) {
        if (partie.joueurActif == partie.monId) {
            jouerTourVersObjectif();
            afficherCartesEnMain();
        } else {
            if (GetMove() != ALL_GOOD) {
                printf(" Erreur pendant le tour de l'adversaire.\n");
                break;
            }
        }

        // afficher un avertissement si les wagons sont faibles
        Joueur* moi = &partie.joueurs[partie.monId];
        Joueur* adv = &partie.joueurs[1 - partie.monId];
        if (moi->nbWagons <= 2 || adv->nbWagons <= 2) {
            printf(" Un des joueurs a 2 wagons ou moins. Fin proche !\n");
        }

    }
}



int main() {
    GameData gameData = {0}; // Initialiser à zéro

    printf("===  TICKET TO RIDE - DÉMARRAGE ===\n");
    
    printf("\n=== Étape 1: Connexion ===\n");
    if (Connexion("Karim") != ALL_GOOD)
        return EXIT_FAILURE;

    printf("\n=== Étape 2: Paramètres ===\n");
    if (SendParameters(&gameData) != ALL_GOOD)
        return EXIT_FAILURE;

    printf("\n=== Étape 3: Affichage plateau ===\n");
    printBoard();


    // Phase initiale : objectifs + première action normale pour chaque joueur
    gererPhaseInitiale();

    //Boucle de jeu principale
    if (partie.phaseInitialeTerminee) {
        boucleDeJeuPrincipale();
    }



    printf("On a plus de 50 CARTES ! ");
    printf("J'ai %d cartes ! ",partie.joueurs[0].nbCartes);

    printf("\n===  FIN DE PARTIE ===\n");
    
    afficherCartesEnMain();
    
    printf("\n\n L'adversaire lui reste : %d\n\n ",partie.joueurs[1].nbWagons);
    

    //afficherRoutes();
    //printBoard();

    quitGame();
    return EXIT_SUCCESS;
}

///tout fonctionne sauf la boucle principale et le drawVISIBLEcard !! 

//Ensuite structurer bien les donnes pour faire un dishtrat qui va m aider a trouver le chemin le plus court et commencer a le prendre le chemin apres avoir pris un objectif le plus POINTS!
    
