/**
 * \file sh13.c
 * \brief Sherlock 13 Game client
 * \author Quentin Deschamps
 * \version 1.0
 * \date Mai 2020
 *
 * Sherlock 13 Game client
 *
 */

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

pthread_t thread_serveur_tcp_id;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char gbuffer[256];
char gServerIpAddress[256];
int gServerPort;
char gClientIpAddress[256];
int gClientPort;
char gName[256];		// Nom du joueur
char gNames[4][256];	// Noms des joueurs
int gId;		// Id du joueur
int joueurSel;	// Joueur sélectionné
int objetSel;	// Objet sélectionné
int guiltSel;	// Coupable sélectionné
int guiltGuess[13];		// Tableau des croix à droite des perso
int tableCartes[4][8];	// Tableau des objets
int b[3];	// Cartes possédées
int goEnabled;	// Bouton go activé
int connectEnabled;	// Bouton connect activé
int replayEnabled;	// Bouton replay activé
int emojiEnabled;	// Emoji activés
int emojiPlayers[4];	// Liste des emojis des joueurs
int nbReplayPlayers;	// Nombre de joueurs qui veulent rejouer

int joueurCourant;	// Numéro du joueur courant
int eliminated[4];	// Liste de 0 et 1 : 1 si joueur éliminé, 0 sinon
int nbPlayersRemaining;	// Nombre de joueurs non élimminés
int guilty;	// Numéro du coupable

char *nbobjets[] = {"5", "5", "5", "5", "4", "3", "3", "3"};
char *nbnoms[] = {
	"Sebastian Moran",
	"Irene Adler",
	"Inspector Lestrade",
	"Inspector Gregson",
	"Inspector Baynes",
	"Inspector Bradstreet",
	"Inspector Hopkins",
	"Sherlock Holmes",
	"John Watson",
	"Mycroft Holmes",
	"Mrs. Hudson",
	"Mary Morstan",
	"James Moriarty"};

volatile int synchro;	// Volatile -> dernière vraie valeur de la variable

void *fn_serveur_tcp(void *arg) {
	/* Gère le thread */
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printf("sockfd error\n");
		exit(1);
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = gClientPort;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("bind error\n");
        exit(1);
    }
	listen(sockfd, 5);
    clilen = sizeof(cli_addr);
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0) {
			printf("accept error\n");
			exit(1);
		}
		bzero(gbuffer, 256);
		n = read(newsockfd, gbuffer, 255);
		if (n < 0) {
			printf("read error\n");
			exit(1);
		}
		// printf("%s",gbuffer);
		synchro = 1;
		while (synchro);
     }
}

void sendMessageToServer(char *ipAddress, int portno, char *mess) {
	/* Envoie un message au serveur */
    int sockfd, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char sendbuffer[256];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    server = gethostbyname(ipAddress);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
    		(char *)&serv_addr.sin_addr.s_addr,
    		server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("ERROR connecting\n");
        exit(1);
    }

    sprintf(sendbuffer, "%s\n", mess);
    n = write(sockfd, sendbuffer, strlen(sendbuffer));

    close(sockfd);
}

void initEmoji() {
	/* Initialise la liste des emojis */
	for (int i = 0 ; i < 4 ; i++)
		emojiPlayers[i] = -1;
}

void initGame() {
	/* Initialise les données pour une partie */
	// Init variables sélection
	int i, j;
	joueurSel = -1;
	objetSel = -1;
	guiltSel = -1;

	// Init cartes joueur
	b[0] = -1;
	b[1] = -1;
	b[2] = -1;

	// Init croix
	for (i = 0 ; i < 13 ; i++)
		guiltGuess[i] = 0;

	// Init table des objets
	for (i = 0 ; i < 4 ; i++)
		for (j = 0 ; j < 8 ; j++)
			tableCartes[i][j] = -1;

	// Init joueurs éliminés
	for (i = 0 ; i < 4 ; i++)
		eliminated[i] = 0;
	nbPlayersRemaining = 4;
	guilty = -1;
	joueurCourant = -1;
}

void resetSel() {
	/* Reset les sélections */
	joueurSel = -1;
	objetSel = -1;
	guiltSel = -1;
}

void initReplay() {
	/* Initialise la demande de replay */
	replayEnabled = 1;
	nbReplayPlayers = 0;
}

int main(int argc, char** argv) {
	int ret;
	int i, j, n;

    int quit = 0;
    SDL_Event event;
	int mx, my;
	char sendBuffer[1024];
	char lname[256];
	char info[300];	// Bandeau des infos

    if (argc < 6) {
        printf("<app> <Main server ip address> <Main server port> <Client ip address> <Client port> <player name>\n");
        exit(1);
    }

	strcpy(gServerIpAddress, argv[1]);
	gServerPort = atoi(argv[2]);
	strcpy(gClientIpAddress, argv[3]);
	gClientPort = atoi(argv[4]);
	strcpy(gName, argv[5]);

    SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();
 
    SDL_Window * window = SDL_CreateWindow(
		"SDL2 SH13",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		1024, 768, 0);
 
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

	// Couleurs
    SDL_Color black = {0, 0, 0};
	SDL_Color red = {204, 78, 60};
	SDL_Color green = {47, 182, 113};

	// Création des surfaces
    SDL_Surface *deck[13], *objet[8], *gobutton, *connectbutton, *replaybutton, *title, *emoji[4];

	deck[0] = IMG_Load("SH13_0.png");
	deck[1] = IMG_Load("SH13_1.png");
	deck[2] = IMG_Load("SH13_2.png");
	deck[3] = IMG_Load("SH13_3.png");
	deck[4] = IMG_Load("SH13_4.png");
	deck[5] = IMG_Load("SH13_5.png");
	deck[6] = IMG_Load("SH13_6.png");
	deck[7] = IMG_Load("SH13_7.png");
	deck[8] = IMG_Load("SH13_8.png");
	deck[9] = IMG_Load("SH13_9.png");
	deck[10] = IMG_Load("SH13_10.png");
	deck[11] = IMG_Load("SH13_11.png");
	deck[12] = IMG_Load("SH13_12.png");

	objet[0] = IMG_Load("SH13_pipe_120x120.png");
	objet[1] = IMG_Load("SH13_ampoule_120x120.png");
	objet[2] = IMG_Load("SH13_poing_120x120.png");
	objet[3] = IMG_Load("SH13_couronne_120x120.png");
	objet[4] = IMG_Load("SH13_carnet_120x120.png");
	objet[5] = IMG_Load("SH13_collier_120x120.png");
	objet[6] = IMG_Load("SH13_oeil_120x120.png");
	objet[7] = IMG_Load("SH13_crane_120x120.png");

	gobutton = IMG_Load("gobutton.png");
	connectbutton = IMG_Load("connectbutton.png");
	replaybutton = IMG_Load("replaybutton.png");
	title = IMG_Load("SH13_title.png");

	emoji[0] = IMG_Load("emoji_detective.png");
	emoji[1] = IMG_Load("emoji_laugh.png");
	emoji[2] = IMG_Load("emoji_worried.png");
	emoji[3] = IMG_Load("emoji_angry.png");

	// Init noms
	strcpy(gNames[0], "-");
	strcpy(gNames[1], "-");
	strcpy(gNames[2], "-");
	strcpy(gNames[3], "-");

	// Init emojis joueurs
	initEmoji();

	// Init Game
	initGame();

	// Init boutons + emojis
	goEnabled = 0;
	connectEnabled = 1;
	replayEnabled = 0;
	emojiEnabled = 0;

	// Création des textures
    SDL_Texture *texture_deck[13], *texture_objet[8], *texture_gobutton, *texture_connectbutton, *texture_replaybutton, *texture_title, *texture_emoji[4];

	for (i = 0 ; i < 13 ; i++)
		texture_deck[i] = SDL_CreateTextureFromSurface(renderer, deck[i]);
	for (i = 0 ; i < 8 ; i++)
		texture_objet[i] = SDL_CreateTextureFromSurface(renderer, objet[i]);

    texture_gobutton = SDL_CreateTextureFromSurface(renderer, gobutton);
    texture_connectbutton = SDL_CreateTextureFromSurface(renderer, connectbutton);
    texture_replaybutton = SDL_CreateTextureFromSurface(renderer, replaybutton);
	texture_title = SDL_CreateTextureFromSurface(renderer, title);

	for (i = 0 ; i < 4 ; i++) {
		texture_emoji[i] = SDL_CreateTextureFromSurface(renderer, emoji[i]);
	}

	// Font
    TTF_Font* Sans = TTF_OpenFont("sans.ttf", 15);
    // printf("Sans=%p\n", Sans);

	/* Creation du thread serveur tcp. */
	printf("Creation du thread serveur tcp !\n");
	synchro = 0;
	ret = pthread_create(&thread_serveur_tcp_id, NULL, fn_serveur_tcp, NULL);

	// Bandeau d'informations
	sprintf(info, "Hello %s! Welcome to Sherlock 13!", gName);

    while (!quit) {
		if (SDL_PollEvent(&event)) {	// Si il y a un évènement
			switch (event.type) {
				case SDL_QUIT:	// Evènement de sortie
					quit = 1;
					break;
				case SDL_MOUSEBUTTONDOWN:	// Evènement de clic avec la souris
					SDL_GetMouseState(&mx, &my);	// Position de la souris
					// printf("mx=%d my=%d\n", mx, my);
					if ((mx < 200) && (my < 50)) {	// En haut à gauche
						if (connectEnabled == 1) {	// Connect button
							// Message 'C' : Connexion
							sprintf(sendBuffer, "C %s %d %s", gClientIpAddress, gClientPort, gName);
							sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
							connectEnabled = 0;
						}
						else if (replayEnabled == 1) {	// Replay button
							initGame();
							// Message 'R' : Replay (demande d'un joueur de rejouer)
							sprintf(sendBuffer, "R %d", gId);
							sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
							replayEnabled = 0;
						}
					}
					else if ((mx >= 0) && (mx < 200) && (my >= 90) && (my < 330)) {
						joueurSel = (my - 90) / 60;
						guiltSel = -1;
					}
					else if ((mx >= 200) && (mx < 680) && (my >= 0) && (my < 90)) {
						objetSel = (mx - 200) / 60;
						guiltSel = -1;
					}
					else if ((mx >= 100) && (mx < 250) && (my >= 350) && (my < 740)) {
						joueurSel = -1;
						objetSel = -1;
						guiltSel = (my - 350) / 30;
					}
					else if ((mx >= 250) && (mx < 300) && (my >= 350) && (my < 740)) {
						int ind = (my - 350) / 30;
						guiltGuess[ind] = 1 - guiltGuess[ind];
					}
					else if ((mx >= 750) && (my >= 700) && (mx < 750 + 50*4) && (my < 700 + 50) && (emojiEnabled == 1)) {
						int ind = (mx - 750) / 50;
						// Message 'H' : Head (envoi d'un emoji)
						sprintf(sendBuffer, "H %d %d", gId, ind);
						sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
					}
					else if ((mx >= 800) && (mx < 900) && (my >= 570) && (my < 670) && (goEnabled == 1)) {
						// printf("go! joueur=%d objet=%d guilt=%d\n", joueurSel, objetSel, guiltSel);
						if (guiltSel != -1) {
							// Message 'G' : Guilt (proposition de coupable)
							sprintf(sendBuffer, "G %d %d", gId, guiltSel);
							sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
							goEnabled = 0;
						}
						else if ((objetSel != -1) && (joueurSel == -1)) {
							// Message 'O' : Objet (demande d'objet à tout le monde)
							sprintf(sendBuffer, "O %d %d", gId, objetSel);
							sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
							goEnabled = 0;
						}
						else if ((objetSel != -1) && (joueurSel != -1)) {
							// Message 'S' : Symbole (demande du nombre d'un symbole à un seul joueur)
							sprintf(sendBuffer, "S %d %d %d", gId, joueurSel, objetSel);
							sendMessageToServer(gServerIpAddress, gServerPort, sendBuffer);
							goEnabled = 0;
						}
					}
					else {	// Clic dans le vide -> reset
						resetSel();
					}
					break;
				case SDL_MOUSEMOTION:	// Evenement déplacement souris
					SDL_GetMouseState(&mx, &my);
					break;
			}
		}

		if (synchro == 1) {		// Si évènement réseau
			printf("consomme: %s", gbuffer);
			switch (gbuffer[0]) {
				// Message 'I' : le joueur recoit son Id
				case 'I':
					sscanf(gbuffer, "I %d", &gId);
					break;
				// Message 'L' : le joueur recoit la liste des joueurs
				case 'L':
					sscanf(gbuffer, "L %s %s %s %s",
						gNames[0], gNames[1], gNames[2], gNames[3]);
					sprintf(info, "You are connected! Waiting for other players...");
					break;
				// Message 'D' : le joueur recoit ses trois cartes
				case 'D':
					sscanf(gbuffer, "D %d %d %d", &b[0], &b[1], &b[2]);
					sprintf(info, "You received your cards! Good luck!");
					break;
				// Message 'M' : le joueur recoit le n° du joueur courant
				// Cela permet d'affecter goEnabled pour autoriser l'affichage du bouton go
				case 'M':
					sscanf(gbuffer, "M %d", &joueurCourant);
					goEnabled = joueurCourant == gId ? 1 : 0;
					emojiEnabled = 1;
					break;
				// Message 'V' : le joueur recoit une valeur de tableCartes
				case 'V':
					sscanf(gbuffer, "V %d %d %d", &i, &j, &n);
					if (tableCartes[i][j] == -1 || tableCartes[i][j] == 0 || tableCartes[i][j] == 100)
						tableCartes[i][j] = n;
					break;
				// Message 'E' : le joueur recoit le numéro du joueur éliminé
				case 'E':
					sscanf(gbuffer, "E %d %d", &i, &j);
					sprintf(info, "%s is eliminated! %s is innocent!", gNames[i], nbnoms[j]);
					eliminated[i] = 1;
					nbPlayersRemaining--;
					guiltGuess[j] = 1;
					break;
				// Message 'W' : le joueur recoit le numéro du joueur victorieux et le coupable
				case 'W':
					sscanf(gbuffer, "W %d %d", &i, &guilty);
					sprintf(info, "%s wins! The guilty person was %s!", gNames[i], nbnoms[guilty]);
					joueurCourant = -1;
					initReplay();
					resetSel();
					break;
				// Message 'H' -> le joueur reçois un emoji avec le numéro du joueur et le numéro d'emoji
				case 'H':
					sscanf(gbuffer, "H %d %d", &i, &j);
					emojiPlayers[i] = j;
					break;
				// Message 'R' -> le joueur recoit une demande de replay d'un joueur
				case 'R':
					sscanf(gbuffer, "R %d", &i);
					nbReplayPlayers++;
					sprintf(info, "%s wants to replay! (%d/4)", gNames[i], nbReplayPlayers);
					break;
			}
			synchro = 0;
		}

		// Maj affichage
		SDL_Rect dstrect_grille = {512 - 250, 10, 500, 350};
		SDL_Rect dstrect_image = {0, 0, 500, 330};
		SDL_Rect dstrect_image1 = {0, 340, 250, 330 / 2};

		// Fond
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_Rect rect = {0, 0, 1024, 768};
		SDL_RenderFillRect(renderer, &rect);

		// Title
		SDL_Rect dstrect_title = {320, 350, 350, 350};
		SDL_RenderCopy(renderer, texture_title, NULL, &dstrect_title);

		// Joueur sélectionné
		if (joueurSel != -1) {
			SDL_SetRenderDrawColor(renderer, 255, 180, 180, 255);
			SDL_Rect rect1 = {0, 90 + joueurSel*60, 200, 60};
			SDL_RenderFillRect(renderer, &rect1);
		}	

		// Objet sélectionné
		if (objetSel != -1) {
			SDL_SetRenderDrawColor(renderer, 180, 255, 180, 255);
			SDL_Rect rect1 = {200 + objetSel*60, 0, 60, 90};
			SDL_RenderFillRect(renderer, &rect1);
		}	

		// Personnage sélectionné
		if (guiltSel != -1) {
			SDL_SetRenderDrawColor(renderer, 180, 180, 255, 255);
			SDL_Rect rect1 = {100, 350 + guiltSel*30, 150, 30}; 
			SDL_RenderFillRect(renderer, &rect1);
		}

		// Images des objets en haut
		{
			SDL_Rect dstrect_pipe = {210, 10, 40, 40};
			SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
			SDL_Rect dstrect_ampoule = {270, 10, 40, 40};
			SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
			SDL_Rect dstrect_poing = {330, 10, 40, 40};
			SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
			SDL_Rect dstrect_couronne = {390, 10, 40, 40};
			SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
			SDL_Rect dstrect_carnet = {450, 10, 40, 40};
			SDL_RenderCopy(renderer, texture_objet[4], NULL, &dstrect_carnet);
			SDL_Rect dstrect_collier = {510, 10, 40, 40};
			SDL_RenderCopy(renderer, texture_objet[5], NULL, &dstrect_collier);
			SDL_Rect dstrect_oeil = {570, 10, 40, 40};
			SDL_RenderCopy(renderer, texture_objet[6], NULL, &dstrect_oeil);
			SDL_Rect dstrect_crane = {630, 10, 40, 40};
			SDL_RenderCopy(renderer, texture_objet[7], NULL, &dstrect_crane);
		}

		// Numéros sous les images des objets
		for (i = 0 ; i < 8 ; i++) {
			SDL_Surface* surfaceMessage = TTF_RenderText_Solid(Sans, nbobjets[i], black);
			SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

			SDL_Rect Message_rect;	// create a rect
			Message_rect.x = 230 + i*60;	// controls the rect's x coordinate 
			Message_rect.y = 50;	// controls the rect's y coordinte
			Message_rect.w = surfaceMessage->w;		// controls the width of the rect
			Message_rect.h = surfaceMessage->h;		// controls the height of the rect

			SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
			SDL_DestroyTexture(Message);
			SDL_FreeSurface(surfaceMessage);
		}

		// Noms des personnages
		for (i = 0 ; i < 13 ; i++) {
			SDL_Surface* surfaceMessage;
			// Nom du coupable en vert si partie terminée
			surfaceMessage = i == guilty ? TTF_RenderText_Solid(Sans, nbnoms[i], green) : TTF_RenderText_Solid(Sans, nbnoms[i], black);
			SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

			SDL_Rect Message_rect;
			Message_rect.x = 105;
			Message_rect.y = 350 + i*30;
			Message_rect.w = surfaceMessage->w;
			Message_rect.h = surfaceMessage->h;

			SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
			SDL_DestroyTexture(Message);
			SDL_FreeSurface(surfaceMessage);
		}

		// Numéros dans le tableau des objets
		for (i = 0 ; i < 4 ; i++)
			for (j = 0 ; j < 8 ; j++) {
				if (tableCartes[i][j] != -1) {
					char mess[10];
					if (tableCartes[i][j] == 100)
						sprintf(mess,"*");
					else
						sprintf(mess,"%d",tableCartes[i][j]);
						SDL_Surface* surfaceMessage = TTF_RenderText_Solid(Sans, mess, black);
						SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

						SDL_Rect Message_rect;
						Message_rect.x = 230 + j*60;
						Message_rect.y = 110 + i*60;
						Message_rect.w = surfaceMessage->w;
						Message_rect.h = surfaceMessage->h;

						SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
						SDL_DestroyTexture(Message);
						SDL_FreeSurface(surfaceMessage);
				}
			}

		// Images des objets à gauche des noms des personnages
		// Sebastian Moran
		{
			SDL_Rect dstrect_crane = {0, 350, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[7], NULL, &dstrect_crane);
		}
		{
			SDL_Rect dstrect_poing = {30, 350, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
		}

		// Irene Adler
		{
			SDL_Rect dstrect_crane = {0, 380, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[7], NULL, &dstrect_crane);
		}
		{
			SDL_Rect dstrect_ampoule = {30, 380, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
		}
		{
			SDL_Rect dstrect_collier = {60, 380, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[5], NULL, &dstrect_collier);
		}

		// Inspector Lestrade
		{
			SDL_Rect dstrect_couronne = {0, 410, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
		}
		{
			SDL_Rect dstrect_oeil = {30, 410, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[6], NULL, &dstrect_oeil);
		}
		{
			SDL_Rect dstrect_carnet = {60, 410, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[4], NULL, &dstrect_carnet);
		}

		// Inspector Gregson 
		{
			SDL_Rect dstrect_couronne = {0, 440, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
		}
		{
			SDL_Rect dstrect_poing = {30, 440, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
		}
		{
			SDL_Rect dstrect_carnet = {60, 440, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[4], NULL, &dstrect_carnet);
		}

		// Inspector Baynes 
		{
			SDL_Rect dstrect_couronne = {0, 470, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
		}
		{
			SDL_Rect dstrect_ampoule = {30, 470, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
		}

		// Inspector Bradstreet
		{
			SDL_Rect dstrect_couronne = {0, 500, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
		}
		{
			SDL_Rect dstrect_poing = {30, 500, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
		}

		// Inspector Hopkins 
		{
			SDL_Rect dstrect_couronne = {0, 530, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[3], NULL, &dstrect_couronne);
		}
		{
			SDL_Rect dstrect_pipe = {30, 530, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
		}
		{
			SDL_Rect dstrect_oeil = {60, 530, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[6], NULL, &dstrect_oeil);
		}

		// Sherlock Holmes
		{
			SDL_Rect dstrect_pipe = {0, 560, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
		}
		{
			SDL_Rect dstrect_ampoule = {30, 560, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
		}
		{
			SDL_Rect dstrect_poing = {60, 560, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
		}

		// John Watson 
		{
			SDL_Rect dstrect_pipe = {0, 590, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
		}
		{
			SDL_Rect dstrect_oeil = {30, 590, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[6], NULL, &dstrect_oeil);
		}
		{
			SDL_Rect dstrect_poing = {60, 590, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[2], NULL, &dstrect_poing);
		}

		// Mycroft Holmes
		{
			SDL_Rect dstrect_pipe = {0, 620, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
		}
		{
			SDL_Rect dstrect_ampoule = {30, 620, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
		}
		{
			SDL_Rect dstrect_carnet = {60, 620, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[4], NULL, &dstrect_carnet);
		}

		// Mrs. Hudson
		{
			SDL_Rect dstrect_pipe = {0, 650, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[0], NULL, &dstrect_pipe);
		}
		{
			SDL_Rect dstrect_collier = {30, 650, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[5], NULL, &dstrect_collier);
		}

		// Mary Morstan
		{
			SDL_Rect dstrect_carnet = {0, 680, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[4], NULL, &dstrect_carnet);
		}
		{
			SDL_Rect dstrect_collier = {30, 680, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[5], NULL, &dstrect_collier);
		}

		// James Moriarty
		{
			SDL_Rect dstrect_crane = {0, 710, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[7], NULL, &dstrect_crane);
		}
		{
			SDL_Rect dstrect_ampoule = {30, 710, 30, 30};
			SDL_RenderCopy(renderer, texture_objet[1], NULL, &dstrect_ampoule);
		}

		// Affichage des emojis
		if (emojiEnabled == 1) {
			{
				SDL_Rect dstrect_emoji_detective = {750, 700, 50, 50};
				SDL_RenderCopy(renderer, texture_emoji[0], NULL, &dstrect_emoji_detective);
			}
			{
				SDL_Rect dstrect_emoji_laugh = {800, 700, 50, 50};
				SDL_RenderCopy(renderer, texture_emoji[1], NULL, &dstrect_emoji_laugh);
			}
			{
				SDL_Rect dstrect_emoji_worried = {850, 700, 50, 50};
				SDL_RenderCopy(renderer, texture_emoji[2], NULL, &dstrect_emoji_worried);
			}
			{
				SDL_Rect dstrect_emoji_angry = {900, 700, 50, 50};
				SDL_RenderCopy(renderer, texture_emoji[3], NULL, &dstrect_emoji_angry);
			}
		}

		SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

		// Afficher les suppositions (les croix du tableau des personnages)
		for (i = 0 ; i < 13 ; i++) {
			if (guiltGuess[i]) {
				SDL_RenderDrawLine(renderer, 250, 350 + i*30, 300, 380 + i*30);
				SDL_RenderDrawLine(renderer, 250, 380 + i*30, 300, 350 + i*30);
			}
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderDrawLine(renderer, 0, 30 + 60, 680, 30 + 60);
		SDL_RenderDrawLine(renderer, 0, 30 + 120, 680, 30 + 120);
		SDL_RenderDrawLine(renderer, 0, 30 + 180, 680, 30 + 180);
		SDL_RenderDrawLine(renderer, 0, 30 + 240, 680, 30 + 240);
		SDL_RenderDrawLine(renderer, 0, 30 + 300, 680, 30 + 300);

		SDL_RenderDrawLine(renderer, 200, 0, 200, 330);
		SDL_RenderDrawLine(renderer, 260, 0, 260, 330);
		SDL_RenderDrawLine(renderer, 320 , 0, 320, 330);
		SDL_RenderDrawLine(renderer, 380, 0, 380, 330);
		SDL_RenderDrawLine(renderer, 440, 0, 440, 330);
		SDL_RenderDrawLine(renderer, 500, 0, 500, 330);
		SDL_RenderDrawLine(renderer, 560, 0, 560, 330);
		SDL_RenderDrawLine(renderer, 620, 0, 620, 330);
		SDL_RenderDrawLine(renderer, 680, 0, 680, 330);

		for (i = 0 ; i < 14 ; i++)
			SDL_RenderDrawLine(renderer, 0, 350 + i*30, 300, 350 + i*30);
		SDL_RenderDrawLine(renderer, 100, 350, 100, 740);
		SDL_RenderDrawLine(renderer, 250, 350, 250, 740);
		SDL_RenderDrawLine(renderer, 300, 350, 300, 740);

		// Images des cartes possédées
		if (b[0] != -1) {
			SDL_Rect dstrect = {750, 0, 1000/4, 660/4};
			SDL_RenderCopy(renderer, texture_deck[b[0]], NULL, &dstrect);
		}
		if (b[1] != -1) {
			SDL_Rect dstrect = {750, 200, 1000/4, 660/4};
			SDL_RenderCopy(renderer, texture_deck[b[1]], NULL, &dstrect);
		}
		if (b[2] != -1) {
			SDL_Rect dstrect = {750, 400, 1000/4, 660/4};
			SDL_RenderCopy(renderer, texture_deck[b[2]], NULL, &dstrect);
		}

		// Le bouton go
		if (goEnabled == 1) {
			SDL_Rect dstrect = {800, 570, 100, 100};
			SDL_RenderCopy(renderer, texture_gobutton, NULL, &dstrect);
		}
		// Le bouton connect
		if (connectEnabled == 1) {
			SDL_Rect dstrect = {0, 0, 200, 50};
			SDL_RenderCopy(renderer, texture_connectbutton, NULL, &dstrect);
		}
		// Le bouton replay
		else if (replayEnabled == 1) {
			SDL_Rect dstrect = {0, 0, 200, 50};
			SDL_RenderCopy(renderer, texture_replaybutton, NULL, &dstrect);
		}

		// Noms des joueurs + emojis
		for (i = 0 ; i < 4 ; i++)
			if (strlen(gNames[i]) > 0) {
				// Nom joueur
				SDL_Surface* surfaceMessage;
				if (eliminated[i]) {	// Si éliminé, en rouge
					surfaceMessage = TTF_RenderText_Solid(Sans, gNames[i], red);
				}
				else {	// Sinon, si joueur courant, en vert. Sinon, en noir.
					surfaceMessage = i == joueurCourant ? TTF_RenderText_Solid(Sans, gNames[i], green) : TTF_RenderText_Solid(Sans, gNames[i], black);
				}
				SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);

				SDL_Rect Message_rect;	// create a rect
				Message_rect.x = 10;	// controls the rect's x coordinate 
				Message_rect.y = 110 + i*60;	// controls the rect's y coordinte
				Message_rect.w = surfaceMessage->w;	// controls the width of the rect
				Message_rect.h = surfaceMessage->h;	// controls the height of the rect

				SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
				SDL_DestroyTexture(Message);
				SDL_FreeSurface(surfaceMessage);

				if (emojiPlayers[i] != -1) {
					// Emoji
					SDL_Rect Emoji_rect;
					Emoji_rect.x = 150;
					Emoji_rect.y = 100 + i*60;
					Emoji_rect.w = 40;
					Emoji_rect.h = 40;
					SDL_RenderCopy(renderer, texture_emoji[emojiPlayers[i]], NULL, &Emoji_rect);
				}
			}

		// Bandeau des informations
		SDL_Surface* surfaceMessage = TTF_RenderText_Solid(Sans, info, black);
		SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
		SDL_Rect Message_rect;
		Message_rect.x = 350;
		Message_rect.y = 720;
		Message_rect.w = surfaceMessage->w;
		Message_rect.h = surfaceMessage->h;
		SDL_RenderCopy(renderer, Message, NULL, &Message_rect);
		SDL_DestroyTexture(Message);
		SDL_FreeSurface(surfaceMessage);

        SDL_RenderPresent(renderer);
    }

	// Destroy & free
	for (i = 0 ; i < 13 ; i++) {
	    SDL_DestroyTexture(texture_deck[i]);
    	SDL_FreeSurface(deck[i]);
	}
	for (i = 0 ; i < 8 ; i++) {
		SDL_DestroyTexture(texture_objet[i]);
    	SDL_FreeSurface(objet[i]);
	}
	for (i = 0 ; i < 4 ; i++) {
		SDL_DestroyTexture(texture_emoji[i]);
		SDL_FreeSurface(emoji[i]);
	}
	SDL_DestroyTexture(texture_gobutton);
    SDL_FreeSurface(gobutton);
	SDL_DestroyTexture(texture_connectbutton);
    SDL_FreeSurface(connectbutton);
	SDL_DestroyTexture(texture_replaybutton);
    SDL_FreeSurface(replaybutton);
	SDL_DestroyTexture(texture_title);
    SDL_FreeSurface(title);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    return 0;
}
