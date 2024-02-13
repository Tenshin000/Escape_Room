// Progetto: ESCAPE ROOM
// Autore: Francesco Panattoni
// Matricola: 604230

// Il main si trova alla riga 3783

// LIBRERIE
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>


// COSTANTI
#define MAX_SERVERS 11 // Numero massimo di Server attivi (compreso Main Server)
#define MAX_CLIENTS 5 // Numero massimo di Client per Game Server
#define N_ROOMS 2 // Numero di Escape Room implementabili
#define MIN_ACCESSIBLE_PORT 1024 // Numero minimo di porta inseribile
#define WAITING 10 // Attesa di 10 secondi quando tutti sono pronti
#define TIME 1200 // 20 minuti
#define MAX_ITEMS 10 // Numero massimo di oggetti nell'inventario
#define BUFFER_SIZE 1024


// STRUTTURE
struct riddle{
    char* question; // Ci si trova l'indovinello, se però è di tipo 2 contiene il nuovo nome che l'oggetto ottiene
    int type; // 0 per risposta a domanda aperta, 1 per risposta a scelta multipla, 2 che c'è bisogno di un oggetto per risolverlo
    char* solution; // La soluzione all'indovinello, nel caso il tipo sia 2 può essere il nome di un oggetto
    char* options[4]; // Array per le opzioni in caso di risposta a scelta multipla
    int attempts_left; // Numero di tentativi rimasti per risolvere l'indovinello, vale solo se è di tipo 1
};

struct object{
    char* name;
    char* description;
    int type; // 0 per chiave, 1 per oggetto inutile, 2 per trappola, 3 per oggetto speciale
    int riddle_answer; // 0 se l'indovinello non è presente o è stato risposto, 1 se c'è un indovinello da rispondere 
    struct riddle enigma;
    int player; // Quale player lo sta utilizzando
    int usable; // Se è utilizzabile 1 altrimenti 0
    int bonus_points; // Punti guadagnati
    int bonus_time; // Tempo bonus guadagnato se l'oggetto viene sbloccato
    int opens_door; // Indica se l'oggetto può aprire una porta
    int location_index; // Indice della porta da aprire
};

struct door{
    int next_location; // Indice della location a cui porta la porta
    int is_blocked; // Se la porta è bloccata
};

struct location{
    char* name;
    char* description;
    struct object* items; // Array di oggetti presenti nella location
    int num_items; // Numero di oggetti presenti
    struct door* linked_locations; // Array di location collegate
    int num_doors; // Numero delle porte
    int is_final; // 0 se non è l'ultima location, 1 se è l'ultima location
    int points_needed; // Punti che si hanno bisogno per interagire con gli oggetti
};

struct escape_room_descriptor{
    int type;
    char* theme;
    char* description;
    struct location* rooms; // Array delle varie stanze della escape room
    int num_rooms; // Numero totale di stanze
};

struct server_descriptor{
    int port; // Porta su cui gira il Server
    int sockfd; // Il descrittore del socket
    int clients; // Numero dei Client
    int main; // Se è il Main Server è 1, se è un Game Server è 0
    pid_t pid; // Il process ID del figlio che fa girare il Game Server, nel caso del Main Server è 0
    struct escape_room_descriptor escape_room; // L'implementazione dell'escape room. Serve solo ai Game Server
};

struct player_descriptor{
    int is_set; // Se è 0 non c'è nessun player alla posizione, se è 1 c'è un player
    int fd; // Il descrittore del socket di comunicazione
    char* username; // Lo username del giocatore
    int ready; // Se è 0 non è pronto, se è 1 è pronto
    int points; // I punti fatti dal giocatore
    struct object items[MAX_ITEMS]; // Array di oggetti nell'inventario del giocatore
    int num_items; // Numero attuale di oggetti nell'inventario
    int location; // In che location si trova
};

struct users{
    char* username; // Lo username all'interno del Main Server. Serve per vedere se qualcuno si è già loggato
    int sockfd; // Il descrittore del socket di comunicazione 
    struct users* next; // È una lista
};

struct message{
    char* phrase; // Frase da mandare agli altri utenti
    char* username; // Username di chi lo ha inviato
    int location; // La location di chi riceve il messaggio
    int views[MAX_CLIENTS]; // Chi lo ha visto dei Client presenti nel Game Server
    int type; // Se è 0 il messaggio è di sistema, se è 1 il messaggio arriva da un Client
    struct message* next; // È una lista
};

// VARIABILI GLOBALI
char buffer[BUFFER_SIZE];
struct server_descriptor servers[MAX_SERVERS]; // La struttura globale che serve a tenere informazioni sul Main Server e i Game Server
const char *path_database = "database.txt";
int game_servers_running = 0; // Indicare quanti server sono in esecuzione
volatile sig_atomic_t interrupted = 0; // Flag che indica se il processo figlio dove gira il Game Server è stato interrotto
volatile int timer_expired = 0; // Flag che indica se la partita è finita o se il timer è scaduto
int win = 0; // Flag che indica se la partita è stata persa (se è 0) o è stata vinta (se è 1)

//  DICHIARAZIONE DI FUNZIONI

// Funzioni di Supporto
int my_random(int, int);
void swap_string(char** a, char** b);
void swap_object(struct object*, struct object*);
void shuffle_objects(struct object*, int);
void create_database();
int create_and_bind_socket(const int, struct sockaddr_in*, const int);
void initialize_main_server_descriptor(struct server_descriptor*, const int, const int);
void initialize_game_servers_descriptor(struct server_descriptor*);
int search_room(struct server_descriptor*, const int);
// Escape Room Medievale
int create_medieval_escape_room(struct escape_room_descriptor*);
int initialize_medieval_escape_room(struct escape_room_descriptor*);
int create_medieval_first_location(struct escape_room_descriptor*, const int);
int create_tavolo_piccolo(struct location*);
int create_tavolo_grande(struct location*);
int create_armeria(struct location*);
int create_medieval_torture_location(struct escape_room_descriptor*, const int, const int);
int create_tortura(struct location*, const int);
int create_gogna(struct location*, const int);
int create_armadio(struct location*, const int);
int create_medieval_painting_location(struct escape_room_descriptor*, const int, const int);
int create_armatura(struct location*, const int);
int create_teca(struct location* , const int);
int create_comodino(struct location* , const int);
int create_medieval_library_location(struct escape_room_descriptor*, const int, const int);
int create_tavolo_centrale(struct location*, const int);
int create_scaffale(struct location*, const int);
int create_finestra(struct location*, const int);
int create_medieval_church_location(struct escape_room_descriptor*, const int, const int);
int create_altare(struct location*, const int);
int create_acqua_santiera(struct location*, const int);
int create_piedistallo(struct location*, const int);
int create_medieval_dining_location(struct escape_room_descriptor*, const int, const int);
int create_tavolo_pranzo(struct location*, const int);
int create_enoteca(struct location*, const int);
int create_medieval_last_location(struct escape_room_descriptor*, const int, const int);
int create_forgia(struct location*, const int);
int create_scrigno(struct location*, const int);
int create_stanza_chiavi(struct location*, const int);
// Escape Room Egizia
int create_ancient_egypt_escape_room(struct escape_room_descriptor*);
int initialize_ancient_egypt_escape_room(struct escape_room_descriptor*);
int create_ancient_egypt_object(struct object*, int, int);
int create_ancient_egypt_riddle(struct riddle*, int);
int create_ancient_egypt_tomb(struct object*);
int create_ancient_egypt_description(struct location*);
// Generali Escape Room
int create_escape_room(struct escape_room_descriptor* , const int);
int initialize_escape_room(struct escape_room_descriptor*, const int);
void destroy_escape_room(struct escape_room_descriptor*);
void destroy_object(struct object*);
void rewrite_description(struct location*, const int);

// Funzioni per il Main Server
int start_main_server(const int);
void print_commands();
int handle_server_commands(struct server_descriptor*, fd_set*, const int);
int handle_client(const int, struct users**);
int connection_to_game_server(const int);
void close_main_server(const int);
int signup(const int, struct users**);
int login(const int, struct users**);
void logout(const int, struct users**);
void accidental_logout(const int, struct users**);
int search_username(const char*, struct users** utenti, const int);
int search_user(const char*, const char*, struct users**, const int);
int send_rooms_informations(struct server_descriptor*, const int);
int expose_escape_rooms();
void control_game_servers(struct server_descriptor*);
struct users* create_user(const char*, const int);
void add_user(struct users**, const char*, const int);
void remove_user(struct users**, const char*);
int count_users(const struct users*);
void free_users(struct users **);
int is_process_running(pid_t);
int wake_up_process(pid_t);

// Funzioni per il Game Server
int start_game_server(struct server_descriptor*, const int, const int);
int game_escape_room(struct server_descriptor*, const int, const int);
void close_game_server(const int, const int);
void signal_handler(const int);
void timer_handler(const int);
void initialize_players_descriptor(struct player_descriptor*);
void set_timer(struct itimerval*, const int);
void reset_timer(struct itimerval*, const int);
int presentation(struct player_descriptor*, const int, const int, const int);
int search_player(struct player_descriptor*, const int);
int control_set_players(struct player_descriptor*);
int control_ready_players(struct player_descriptor*);
int is_numeric(const char*);
int count_points(struct player_descriptor*);
int send_object(const int, const struct object*);
struct message* create_message(const char*, const char*, const int, const int, const int);
void add_message(struct message**, const char*, const char*, const int, const int, const int);
void destroy_message(struct message*);
void remove_message(struct message**, struct message*);
void destroy_message_list(struct message**);
int count_pending_messages(const struct message*, const int);
int control_messages(const int, struct message**, const int, struct player_descriptor*);
int handle_client_commands(const int, struct server_descriptor*, const int, struct player_descriptor*, const int, struct message**, struct itimerval*);
int begin(const int, const int, struct itimerval*, struct player_descriptor*, const int);
int quit(const int, const int, struct player_descriptor*, const int, struct itimerval*);
int go_to(const int, const char*, struct server_descriptor*, const int, struct player_descriptor*, const int giocatore, struct message**, struct itimerval*);
int look(const int, const char*, struct server_descriptor*, const int, struct player_descriptor*, const int giocatore, struct message**, struct itimerval*);
int take(const int, const char*, struct server_descriptor*, const int, struct player_descriptor*, const int giocatore, struct message**, struct itimerval*);
int go_on(const int, struct server_descriptor*, const int, struct player_descriptor*, const int giocatore, struct message**, struct itimerval*);
int use(const int, const char*, struct server_descriptor*, const int, struct player_descriptor*, const int giocatore, struct message**, struct itimerval*);
int use_object(const int, const char*, const char*, struct server_descriptor*, const int, struct player_descriptor*, const int giocatore, struct message**, struct itimerval*);
int objs(const int, struct player_descriptor*, const int giocatore, struct message**, struct itimerval*);
int doors(const int, struct server_descriptor*, const int, struct player_descriptor*, const int giocatore, struct message**, struct itimerval*);
int message(const int, struct message**, const char*, struct server_descriptor*, const int, struct player_descriptor*, const int giocatore, struct itimerval*);
int end(const int, struct server_descriptor*, const int, struct player_descriptor*, const int giocatore, struct message**, struct itimerval* timer);
int timeout(const int, struct message**, struct player_descriptor*, const int);
int send_time(const int, struct player_descriptor*, const int, struct message**, struct itimerval*);

// FUNZIONI UTILI

// Custom send che si impegna ad inviare tutti i dati
int send_all(const int sockfd, const void* buf, size_t size, int flags){
    size_t total_sent = 0; 
    ssize_t sent_bytes;     

    while(total_sent < size){
        sent_bytes = send(sockfd, (char*)buf + total_sent, size - total_sent, flags);
        if(sent_bytes < 0){
            perror("Errore durante l'invio. \n");
            return sent_bytes;
        }
        total_sent += sent_bytes;
    }

    return 1; 
}

// Custom recv che si impegna a ricevere tutti i dati
int recv_all(const int sockfd, void* buf, size_t size, int flags){
    size_t total_received = 0; 
    ssize_t received_bytes;    

    while(total_received < size){
        received_bytes = recv(sockfd, (char*)buf + total_received, size - total_received, flags);
        if(received_bytes <= 0){
            if(received_bytes == 0){
                printf("Connessione chiusa dal lato del Client. \n");
                return -1;
            } 
            else{
                perror("Errore durante la ricezione. \n");
                return received_bytes;
            }   
        }
        total_received += received_bytes;
    }

    return total_received; 
}

// Custom send che invia un messaggio
int send_msg(const int sockfd, const char* msg, int length, int flags){
    int ret;

    if(msg == NULL){
        printf("Errore nell'assegnazione del messaggio. \n");
        return -1;
    }

    if(length <= 0 || length > strlen(msg)){
        length = strlen(msg);
        if(length <= 0){
            printf("Errore nell'assegnazione della lunghezza messaggio. \n");
            return -1;
        }
    }

    memset(buffer, 0, length);

    ret = send_all(sockfd, &length, sizeof(length), flags);
    if(ret < 0){
        perror("Errore nell'invio della lunghezza del messaggio. \n");
        return ret;
    }

    strcpy(buffer, msg);
    ret = send_all(sockfd, buffer, length, flags);
    if(ret < 0){
        perror("Errore nell'invio del messaggio. \n");
        return ret;
    }

    memset(buffer, 0, length);

    return 1;
}

// Custom recv che riceve un messaggio
int recv_msg(const int sockfd, char* buf, int flags){
    int ret, length;

    ret = recv_all(sockfd, &length, sizeof(length), flags);
    if(ret < 0){
        perror("Errore nella ricezione della lunghezza del messaggio. \n");
        return ret;
    }

    if(length <= 0){
        printf("Errore nella ricezione della lunghezza del messaggio. \n");
        return -1;
    }

    memset(buf, 0, length+1);

    ret = recv_all(sockfd, buf, length, flags);
    if(ret < 0){
        perror("Errore nella ricezione del messaggio. \n");
        return ret;
    }

    buf[length] = '\0';
    return length;
}

// Funzione per generare un numero casuale compreso tra min e max
int my_random(int min, int max){
    int temp;
    if(min > max){
        temp = min;
        min = max;
        max = temp;
    }

    return rand() % (max - min + 1) + min;
}

// Funzione per scambiare due puntatori a char
void swap_string(char** a, char** b){
    char* temp = *a;
    *a = *b;
    *b = temp;
}

// Funzione per scambiare due puntatori a object
void swap_object(struct object* a, struct object* b){
    struct object temp = *a;
    *a = *b;
    *b = temp;
}

// Funzione che mescola gli oggetti
void shuffle_objects(struct object* items, int size){
    for(int i = size - 1; i > 0; i--){
        // Genera un indice casuale tra 0 e i utilizzando la tua funzione my_random
        int j = my_random(0, i);
        // Scambia le posizioni degli oggetti
        swap_object(&items[i], &items[j]);
    }
}

// Crea il file database.txt
void create_database(){
    FILE *file;

    printf("Caricamento Database giocatori... \n");

    // Prova ad aprire il file in modalità lettura
    file = fopen(path_database, "r");

    // Verifica se il file esiste
    if(file == NULL){
        printf("Creazione Database giocatori... \n");
        // Se il file non esiste, aprire il file in modalità scrittura crea il file
        file = fopen(path_database, "w");

        // Verifica se il file è stato creato correttamente
        if(file != NULL){
            fprintf(file, "DATABASE GIOCATORI \nUSERNAME PASSWORD\n");
            fclose(file); // Chiudi il file dopo averlo utilizzato
        } 
        else{
            printf("Errore durante la creazione del file Database. \n");
            exit(EXIT_FAILURE);
        }
    } 
    else
        fclose(file);
}

// Funzione che crea e fa la bind di un socket di ascolto
int create_and_bind_socket(const int port, struct sockaddr_in* server_address, const int main){
    int listener, enable_reuse = 1, ret;

    // Creazione Socket
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if(listener < 0){
        perror("Errore nella creazione del Socket: \n");
        if(main == 0) // Se è un Game Server esce con EXIT_FAILURE
            exit(EXIT_FAILURE);
        else // Se è un Main Server esce restituendo -1
            return -1;
    }

    // Creazione Indirizzo del Socket
    memset(server_address, 0, sizeof(*server_address));
    server_address->sin_family = AF_INET;
    server_address->sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_address->sin_addr);

    // Per fare in modo di riusare la porta
    ret = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(int));
    if(ret < 0){
        perror("Errore nell'impostazione dell'opzione SO_REUSEADDR");
        close(listener);
        if(main == 0) // Se è un Game Server esce con EXIT_FAILURE
            exit(EXIT_FAILURE);
        else // Se è un Main Server esce restituendo -1
            return -1;
    }

    // Assegnazione dell'indirizzo del Server al socket
    ret = bind(listener, (struct sockaddr*)server_address, sizeof(*server_address));
    if(ret < 0){
        perror("Errore nell'assegnazione dell'Indirizzo");
        if(main == 0) // Se è un Game Server esce con EXIT_FAILURE
            exit(EXIT_FAILURE);
        else // Se è un Main Server esce restituendo -1
            return -1;
    }

    return listener; // Restituisci il descrittore del socket creato e collegato
}

// Inizializza tutti il descrittore del Main Server
void initialize_main_server_descriptor(struct server_descriptor* s, const int sockfd, const int port){
    s[0].sockfd = sockfd;
    s[0].port = port;
    s[0].clients = 0;
    s[0].pid = 0;
    s[0].main = 1;
    s[0].escape_room.theme = NULL;
    s[0].escape_room.description = NULL;  
}

// Inizializza tutti i descrittori dei Game Server
void initialize_game_servers_descriptor(struct server_descriptor* s){
    int i = 1;
    for(; i < MAX_SERVERS; i++){
        s[i].sockfd = -1;
        s[i].port = 0;
        s[i].clients = 0;
        s[i].pid = -1;
        s[i].main = 0;
        s[i].escape_room.theme = NULL;
        s[i].escape_room.description = NULL;  
    }  
}

// Cerca in quale indice si trova la porta "port" e se non la trova restituisce -1
int search_room(struct server_descriptor* s, const int port){
    int i = 0, risultato = -1;

    for(; i < MAX_SERVERS; i++){
        if(s[i].port == port){
            risultato = i;
            break;
        }
    }

    return risultato;
}

// Indice 0: Escape Room Medievale
int create_medieval_escape_room(struct escape_room_descriptor* r){
    int numero_stanze, i = 0, tipo, precedente = -1, prima = -1, ret;

    numero_stanze = my_random(4, 7); // Il numero delle stanze varia da 4 a 7

    ret = initialize_medieval_escape_room(r); // Inizializza il tema, la descrizione e il tipo della Escape Room con quella medievale
    if(ret < 0)
        return ret;

    r->num_rooms = numero_stanze * 4; // Ogni stanza è una location, ma all'interno di una stanza ci sono altre 3 location che fanno da luogo particolare
    r->rooms = (struct location*) malloc(r->num_rooms * sizeof(struct location)); // Creo il vettore delle stanze dell'Escape Room
    if(r->rooms == NULL)
        return -1;

    for(; i < numero_stanze; i++){
        // Creo la prima stanza dell'Escape Room Medievale
        if(i == 0){
            ret = create_medieval_first_location(r, numero_stanze);
            if(ret < 0){
                destroy_escape_room(r);
                return ret;
            }
        }
        // Creo le altre stanze tra la prima e l'ultima nell'Escape Room Medievale
        else if(i > 0 && i < numero_stanze - 1){
            tipo = my_random(1, 5); // Decido quale dei 5 tipi di stanza creo a my_random
            while(tipo == precedente || tipo == prima) // Controllo che nella stanza precedente non ci siano collegate due stanze dello stesso tipo
                tipo = my_random(1, 5); // Se sì la cambio

            if(tipo == 1){
                // Creo la stanza della tortura
                ret = create_medieval_torture_location(r, numero_stanze, i);
                if(ret < 0){
                    destroy_escape_room(r);
                    return ret;
                }
            }
            else if(tipo == 2){
                // Creo la stanza dei quadri
                ret = create_medieval_painting_location(r, numero_stanze, i);
                if(ret < 0){
                    destroy_escape_room(r);
                    return ret;
                }
            }
            else if(tipo == 3){
                ret = create_medieval_library_location(r, numero_stanze, i);
                if(ret < 0){
                    destroy_escape_room(r);
                    return ret;
                }
            }
            else if(tipo == 4){
                // Creo la Chiesetta interna al Castello
                ret = create_medieval_church_location(r, numero_stanze, i);
                if(ret < 0){
                    destroy_escape_room(r);
                    return ret;
                }
            }
            else if(tipo == 5){
                // Creo la stanza da pranzo
                ret = create_medieval_dining_location(r, numero_stanze, i);
                if(ret < 0){
                    destroy_escape_room(r);
                    return ret;
                }
            }

            prima = precedente;
            precedente = tipo;
        }
        // Creo l'ultima stanza dell'Escape Room Medievale
        else if(i == numero_stanze - 1){
            ret = create_medieval_last_location(r, numero_stanze, i);
            if(ret < 0){
                destroy_escape_room(r);
                return ret;
            }
        }
    }
    
    return 0;
}

// Inizializza la Escape Room Medievale
int initialize_medieval_escape_room(struct escape_room_descriptor* r){
    const char* tema = "Escape Room Medievale";
    const char* descrizione = "Escape Room in un antico castello medievale. Dovrai fuggire attraverso le varie sale del castello."; 

    r->theme = (char*) malloc((strlen(tema) + 1)  * sizeof(char));
    if(r->theme== NULL){
        printf("Errore nella creazione della Escape Room Medievale con il codice \n");
        return -1;
    }
    strcpy(r->theme, tema);


    r->description = (char*) malloc((strlen(descrizione) + 1) * sizeof(char));
    if(r->description == NULL){
        printf("Errore nella creazione della Escape Room Medievale con il codice \n");
        return -1;
    }
    strcpy(r->description, descrizione);

    r->type = 0;
    
    return 0;
}

// Crea la prima location della escape room medievale
int create_medieval_first_location(struct escape_room_descriptor* r, const int num_rooms){
    int ret;
    const char* nome_location = "Stanza Iniziale";
    const char* descrizione_location = "Questa stanza polverosa vi dà il benvenuto. Ti trovi nella ++Stanza Iniziale++. \nLa stanza è illuminata da una luce fioca proveniente da candele posizionate su vecchi candelabri di ferro. I muri sono rivestiti di pietra grezza. \nAl suo interno trovate un ++Tavolo Piccolo++ con una teca con dei numeri sopra, un ++Tavolo Grande++ con vari alambicchi sopra e un'++Armeria++. \nVi è un ulteriore porta che vi porterà a ++???++ (visualizzabile con il comando doors).";

    // Stanza Iniziale [0]
    // Crea il nome
    r->rooms[0].name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(r->rooms[0].name== NULL)
        return -1;
    strcpy(r->rooms[0].name, nome_location);

    // Crea la descrizione
    r->rooms[0].description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(r->rooms[0].description == NULL)
        return -1;
    
    strcpy(r->rooms[0].description, descrizione_location);

    // Non ci sono oggetti
    r->rooms[0].num_items = 0;
    r->rooms[0].items = NULL;

    // Ti colleghi a 4 location
    r->rooms[0].num_doors = 4;
    r->rooms[0].linked_locations = (struct door*) malloc(r->rooms->num_doors  * sizeof(struct door));
    if(r->rooms[0].linked_locations  == NULL)
        return -1;
    
    r->rooms[0].linked_locations[0].next_location = 1; // Ti colleghi alla prossima stanza
    r->rooms[0].linked_locations[0].is_blocked = 1; // Ed è bloccata
    r->rooms[0].linked_locations[1].next_location = num_rooms; // Ti colleghi ad una location interna
    r->rooms[0].linked_locations[1].is_blocked = 0; // Non è bloccata
    r->rooms[0].linked_locations[2].next_location = num_rooms + 1; // Ti colleghi ad una location interna
    r->rooms[0].linked_locations[2].is_blocked = 0; // Non è bloccata
    r->rooms[0].linked_locations[3].next_location = num_rooms + 2; // Ti colleghi ad una location interna
    r->rooms[0].linked_locations[3].is_blocked = 0; // Non è bloccata

    r->rooms[0].is_final = 0; // Non è la stanza finale
    r->rooms[0].points_needed = 0; // Non servono punti per interagire
    
    // Crea Tavolo Piccolo [num_rooms]
    ret = create_tavolo_piccolo(&r->rooms[num_rooms]);
    if(ret < 0)
        return ret;
    
    // Crea Tavolo Grande [num_rooms+1]
    ret = create_tavolo_grande(&r->rooms[num_rooms+1]);
    if(ret < 0)
        return ret;
    
    // Crea Armeria [num_rooms+2]
    ret = create_armeria(&r->rooms[num_rooms+2]);
    if(ret < 0)
        return ret;

    return 0;
}

// Crea la location tavolo piccolo
int create_tavolo_piccolo(struct location* l){
    int indice, i = 0;
    int numero = 7; // Il numero di indovinelli all'interno del vettore di stringhe "sequenze" e delle rispettive "soluzioni"

    const char* nome_location = "Tavolo Piccolo";
    const char* descrizione_location = "Questo tavolo polveroso, ha sopra una teca con un codice numerico e dentro una **Chiave**.";
    const char* nome_oggetto = "Chiave";
    const char* descrizione_oggetto = "Per ottenere questa chiave devi completare questa sequenza numerica:";
    
    // Gli indovinelli
    const char* sequenze[] = {
        "1 2 4 7 11 ?", // Aggiungi 1, 2, 3, 4, 5
        "3 6 10 15 21 ?", // Aggiungi 3, 4, 5, 6, 7
        "2 4 8 16 32 ?", // Moltiplichi per 2
        "2 5 10 17 26 ?", // Aggiungi 1, 3, 5, 7, 9
        "1 4 9 16 25 ?", // Quadrato dei numeri 1, 2, 3, 4, 5
        "3 7 13 21 31 ?", // Aggiungi 4, 6, 8, 10, 12
        "2 3 5 8 13 ?" // Somma degli ultimi due numeri
    };

    // Le soluzioni. La prima stringa di "soluzioni" è la soluzione del primo indovinello "sequenze" e così via ...
    const char* soluzioni[] = {
        "16",
        "28",
        "64",
        "37",
        "36",
        "43",
        "21"
    };

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0; // Non è la stanza finale
    l->points_needed = 0; // Non servono punti per interagire

    // Serve per tornare indietro alla stanza da cui si è arrivati
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    
    l->linked_locations[0].next_location = 0; // La prima locazione
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 1; // Apre una porta
    l->items[0].location_index = 1;

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 1;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    indice = my_random(0, numero - 1);
    l->items[0].enigma.type = 0;

    l->items[0].enigma.question = (char*) malloc((strlen(sequenze[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, sequenze[indice]);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzioni[indice]);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location tavolo grande
int create_tavolo_grande(struct location* l){
    int indice, i = 0, j;
    int numero = 7; // Numero degli indovinelli

    const char* nome_location = "Tavolo Grande";
    const char* descrizione_location = "Questo tavolo contiene vari alambicchi e si può, pertanto, creare una **Pozione**.";
    const char* nome_oggetto = "Pozione";
    const char* descrizione_oggetto = "Una strana pozione. Potrebbe avere un effetto benefico.";
    
    const char* domande[] = {
        "Quale dei seguenti frutti galleggia in acqua?",
        "Quale tra le seguenti piante hai il fiore con l'odore più intenso",
        "Quale tra i seguenti numeri è primo?",
        "Quante zampe ha un insetto?",
        "Qual è l'animale simbolo del potere in molte leggende medievali?",
        "Qual è l'unicorno considerato simbolo di?",
        "Quale materiale si supponeva proteggesse da frecce e spade nel Medioevo?"
    };

    const char* soluzioni[] = {
        "Noce di Cocco", // Quale dei seguenti frutti galleggia in acqua?
        "Rafflesia", // Quale tra le seguenti piante hai il fiore con l'odore più intenso
        "2", // Quale tra i seguenti numeri è primo?
        "6", // Quante zampe ha un insetto?
        "Drago", // Qual è l'animale simbolo del potere in molte leggende medievali?
        "Purezza", // Qual è l'unicorno considerato simbolo di?
        "Maglia" // Quale materiale si supponeva proteggesse da frecce e spade nel Medioevo?              
    };

    const char* errata1[] = {
        "Banana", // Quale dei seguenti frutti galleggia in acqua?
        "Rosa", // Quale tra le seguenti piante hai il fiore con l'odore più intenso
        "1", // Quale tra i seguenti numeri è primo?
        "4",// Quante zampe ha un insetto?
        "Grifone", // Qual è l'animale simbolo del potere in molte leggende medievali?
        "Ricchezza", // Qual è l'unicorno considerato simbolo di?
        "Juta" // Quale materiale si supponeva proteggesse da frecce e spade nel Medioevo?
    };

    const char* errata2[] = {
        "Melone", // Quale dei seguenti frutti galleggia in acqua?
        "Gelsomino", // Quale tra le seguenti piante hai il fiore con l'odore più intenso
        "0", // Quale tra i seguenti numeri è primo?
        "8", // Quante zampe ha un insetto?
        "Cerbero", // Qual è l'animale simbolo del potere in molte leggende medievali?
        "Forza", // Qual è l'unicorno considerato simbolo di?
        "Cuoio" // Quale materiale si supponeva proteggesse da frecce e spade nel Medioevo?
    };

    const char* errata3[] = {
        "Anguria", // Quale dei seguenti frutti galleggia in acqua?
        "Orchidea", // Quale tra le seguenti piante hai il fiore con l'odore più intenso
        "27", // Quale tra i seguenti numeri è primo?
        "10", // Quante zampe ha un insetto?
        "Fenice", // Qual è l'animale simbolo del potere in molte leggende medievali?
        "Amore", // Qual è l'unicorno considerato simbolo di?
        "Acciaio" // Quale materiale si supponeva proteggesse da frecce e spade nel Medioevo?
    };

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = 0; // La prima locazione
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;
    
    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 1;

    l->items[0].opens_door = 0; // Non apre porte
    l->items[0].location_index = -1;

    l->items[0].bonus_time = 30; // Tempo extra di 30 secondi
    l->items[0].bonus_points = 0;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    indice = my_random(0, numero - 1);
    l->items[0].enigma.type = 1;

    l->items[0].enigma.question = (char*) malloc((strlen(domande[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    
    strcpy(l->items[0].enigma.question, domande[indice]);

    // Genere le 4 opzioni e le mescola
    l->items[0].enigma.solution = (char*) malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    
    strcpy(l->items[0].enigma.solution, soluzioni[indice]);

    l->items[0].enigma.attempts_left = 3;

    l->items[0].enigma.options[0] = (char*) malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[0] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[0], soluzioni[indice]);

    l->items[0].enigma.options[1] = (char*) malloc((strlen(errata1[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[1] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[1], errata1[indice]);

    l->items[0].enigma.options[2] = (char*) malloc((strlen(errata2[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[2] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[2], errata2[indice]);

    l->items[0].enigma.options[3] = (char*) malloc((strlen(errata3[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[3] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[3], errata3[indice]);
        
    // Mescola le opzioni
    for(; i < 3; i++){
        j = my_random(3, i);
        swap_string(&(l->items[0].enigma.options[i]), &(l->items[0].enigma.options[j]));
    }

    return 0;
}

// Crea la location armeria
int create_armeria(struct location* l){
    int indice, i = 0;

    const char* nome_location = "Armeria";
    const char* descrizione_location = "Questa armeria ha varie armi arrugginite. Tuttavia posi il tuo sguardo su una **Alabarda** scintillante.";
    const char* nome_oggetto = "Alabarda";
    const char* descrizione_oggetto = "Un alabarda splendente e molto affilata. Chissà non possa servire per qualcosa.";
    
    const char* indovinelli[] = {
        "Gollum disse: Verde, liscio, lucido, sottile. Un tesoro più prezioso di un re. Chi sa cosa è?",
        "Bilbo disse: Fuori lo butti, dentro lo tieni, lo trovi perdendo, ma perdi trovandolo. Che cos'è?"
    };

    const char* soluzioni[] = {
        "Uovo",
        "Anello"
    };

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = 0; // La prima locazione
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 1; // Apre una porta // Apre una porta
    l->items[0].location_index = 1; // La prima all'interno della prima location

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 1;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    indice = my_random(0, 1);
    l->items[0].enigma.type = 0;

    l->items[0].enigma.question = (char*) malloc((strlen(indovinelli[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, indovinelli[indice]);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzioni[indice]);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location della stanza della tortura per la escape room medievale 
int create_medieval_torture_location(struct escape_room_descriptor* r, const int num_rooms, const int index){
    int ret;
    const char* nome_location = "Stanza delle Torture";
    const char* descrizione_location = "Questa cupa e lugubre stanza, sembra essere una ++Stanza delle Torture++. \nIn questa stanza sembra trovarsi una ++Tortura della Corda++, una ++Gogna++ e un ++Armadio++ con vari strumenti di tortura. \nVi è anche una porta che vi porterà a ++???++ e la porta che vi riporterà alla location precedente (visualizzabili con il comando doors).";

    // Stanza delle Torture [index]
    r->rooms[index].name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(r->rooms[0].name== NULL)
        return -1;
    strcpy(r->rooms[index].name, nome_location);

    r->rooms[index].description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(r->rooms[index].description == NULL)
        return -1;
    strcpy(r->rooms[index].description, descrizione_location);

    r->rooms[index].num_items = 0;
    r->rooms[index].items = NULL;
    
    // Ti colleghi a 5 location
    r->rooms[index].num_doors = 5;
    r->rooms[index].linked_locations = (struct door*) malloc(r->rooms->num_doors  * sizeof(struct door));
    if(r->rooms[index].linked_locations  == NULL)
        return -1;

    // Quella successiva.
    r->rooms[index].linked_locations[0].next_location = index+1;
    r->rooms[index].linked_locations[0].is_blocked = 1;

    // Quella precedente. La stanza dei quadri è particolare e quindi viene lasciata aperta. Se no è chiusa. 
    r->rooms[index].linked_locations[1].next_location = index-1;
    if(strcmp(r->rooms[index-1].name, "Stanza dei Quadri") == 0)
        r->rooms[index].linked_locations[1].is_blocked = 0;
    else
        r->rooms[index].linked_locations[1].is_blocked = 1;

    // Collegamenti alle location interne
    r->rooms[index].linked_locations[2].next_location = num_rooms + index * 3;
    r->rooms[index].linked_locations[2].is_blocked = 0;
    r->rooms[index].linked_locations[3].next_location = num_rooms + index * 3 + 1;
    r->rooms[index].linked_locations[3].is_blocked = 0;
    r->rooms[index].linked_locations[4].next_location = num_rooms + index * 3 + 2;
    r->rooms[index].linked_locations[4].is_blocked = 0;

    r->rooms[index].is_final = 0; // Non è la stanza finale
    r->rooms[index].points_needed = 0; // Non servono punti per interagire

    // Tortura della Corda [num_rooms + index * 3]
    ret = create_tortura(&r->rooms[num_rooms + index * 3], index);
    if(ret < 0)
        return ret;

    // Gogna [num_rooms + index * 3 + 1]
    ret = create_gogna(&r->rooms[num_rooms + index * 3 + 1], index);
    if(ret < 0)
        return ret;
    
    // Armadio [num_rooms + index * 3 + 2]
    ret = create_armadio(&r->rooms[num_rooms + index * 3 + 2], index);
    if(ret < 0)
        return ret;

    return 0;
}

// Crea la location Tortura della Corda
int create_tortura(struct location* l, const int index){
    int i = 0;

    const char* nome_location = "Tortura della Corda";
    const char* descrizione_location = "Questo strumento di dolore si usa legando le mani della vittima dietro la schiena e sollevando la persona dal terreno tramite una corda legata ai polsi, causando un'estrema tensione e un grandedolore alle spalle, alle braccia e alle articolazioni. \nPuoi usarlo per uno scopo meno cruento, usando la tensione per spezzare qualcosa. Interagisci con l'oggetto **Tortura**";
    const char* nome_oggetto = "Tortura";
    const char* descrizione_oggetto = "Puoi applicare la tensione per ricavarne qualcosa con questo.";
    const char* soluzione = "Chiave_Incastrata";
    const char* nuovo_oggetto = "Chiave";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0; 
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 1; // Apre una porta
    l->items[0].location_index = index+1;

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 1;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 2;

    l->items[0].enigma.question = (char*) malloc((strlen(nuovo_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, nuovo_oggetto);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzione) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzione);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location Gogna
int create_gogna(struct location* l, const int index){
    const char* nome_location = "Gogna";
    const char* descrizione_location = "La gogna era una struttura pubblica con aperture per le mani o la testa, dove le persone venivano messe in mostra e soggette a scherni e umiliazioni da parte della folla. Qua non trovi nulla di che.";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);
    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index;
    l->linked_locations[0].is_blocked = 0;

    // Non ha oggetti
    l->num_items = 0;
    l->items = NULL;

    return 0;
}

// Crea la location armadio
int create_armadio(struct location* l, const int index){
    int indice, i = 0;
    int numero = 4; // Numero degli indovinelli

    const char* nome_location = "Armadio";
    const char* descrizione_location = "In questo armadio sono tenuti vari strumenti di tortura, tra cui tenaglie, fruste, lame, eccetera... \nUn momento. Noti qualcosa di strano ... una **Chiave_Incastrata** in lucchetto rotto. Forse è la chiave per proseguire. ";
    const char* nome_oggetto = "Chiave_Incastrata";
    const char* descrizione_oggetto = "Questa chiave sembra essere incastratta in questo lucchetto sverto da una porta. Non è possibile tirarlo fuori con la propria forza.";

    const char* indovinelli[] = {
        "Chi è che è sempre in ritardo?",
        "Cosa è che va su e giù senza mai muoversi?",
        "Cos'è che ti appartiene, ma viene usato più spesso dagli altri?",
        "Cosa ha la testa e la coda, ma non ha corpo? (La coda è detta anche in altra maniera c***e.)",
    };

    const char* soluzioni[] = {
        "Futuro", // Chi è che è sempre in ritardo?
        "Scale", // Cosa è che va su e giù senza mai muoversi?
        "Nome", // Cos'è che ti appartiene, ma viene usato più spesso dagli altri?
        "Moneta", // Cosa ha la testa e la coda, ma non ha corpo?
    };

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 0;
    l->items[0].location_index = -1;

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 0;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 0;

    indice = my_random(0, numero - 1);
    l->items[0].enigma.type = 0;

    l->items[0].enigma.question = (char*) malloc((strlen(indovinelli[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, indovinelli[indice]);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzioni[indice]);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Funzione per creare la stanza dei quadri nella escape room medievale
int create_medieval_painting_location(struct escape_room_descriptor* r, const int num_rooms, const int index){
    int ret;
    const char* nome_location = "Stanza dei Quadri";
    const char* descrizione_location = "Ti ritrovi nella ++Stanza dei Quadri++ del Castello. Ci sono vari quadri dei possessori del Castello e delle varie famiglie nel tempo, ma non mi sembrano volti noti. \nAll'interno della stanza troviamo un'++Armatura++, una ++Teca++ con il quadro di quello che sembra essere il primo possessore del Castello e un ++Comodino++ con sopra una clessidra. \nVi è anche una porta che vi porterà a ++???++ e la porta che vi riporterà alla location precedente (visualizzabili con il comando doors).";

    // Stanza delle Torture [index]
    r->rooms[index].name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(r->rooms[0].name== NULL)
        return -1;
    strcpy(r->rooms[index].name, nome_location);

    r->rooms[index].description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(r->rooms[index].description == NULL)
        return -1;
    strcpy(r->rooms[index].description, descrizione_location);

    r->rooms[index].num_items = 0;
    r->rooms[index].items = NULL;
    
    // Ti colleghi a 5 location
    r->rooms[index].num_doors = 5;
    r->rooms[index].linked_locations = (struct door*) malloc(r->rooms->num_doors  * sizeof(struct door));
    if(r->rooms[index].linked_locations  == NULL)
        return -1;

    r->rooms[index].linked_locations[0].next_location = index+1; // Locazione successiva
    r->rooms[index].linked_locations[0].is_blocked = 0; // Non è bloccata
    r->rooms[index].linked_locations[1].next_location = index-1; // Locazione precedente
    r->rooms[index].linked_locations[1].is_blocked = 1; // Bloccata
    r->rooms[index].linked_locations[2].next_location = num_rooms + index * 3; // Locazione interna
    r->rooms[index].linked_locations[2].is_blocked = 0; // Non è bloccata
    r->rooms[index].linked_locations[3].next_location = num_rooms + index * 3 + 1; // Locazione interna
    r->rooms[index].linked_locations[3].is_blocked = 0; // Non è bloccata
    r->rooms[index].linked_locations[4].next_location = num_rooms + index * 3 + 2; // Locazione interna
    r->rooms[index].linked_locations[4].is_blocked = 0; // Non è bloccata

    r->rooms[index].is_final = 0;
    r->rooms[index].points_needed = 0;

    // Armatura [num_rooms + index * 3]
    ret = create_armatura(&r->rooms[num_rooms + index * 3], index);
    if(ret < 0)
        return ret;

    // Teca [num_rooms + index * 3 + 1]
    ret = create_teca(&r->rooms[num_rooms + index * 3 + 1], index);
    if(ret < 0)
        return ret;

    // Comodino [num_rooms + index * 3 + 2]
    ret = create_comodino(&r->rooms[num_rooms + index * 3 + 2], index);
    if(ret < 0)
        return ret;

    return 0;
}

// Crea la location armatura
int create_armatura(struct location* l, const int index){
    int i = 0;

    const char* nome_location = "Armatura";
    const char* descrizione_location = "L'armatura è medievale e di ottima fattura. Realizzata con acciaio di alta qualità e composta da piastre per petto, schiena, braccia e gambe. L'elmo include visiera e protezione per testa e collo, garantendo sicurezza e mobilità durante il combattimento.\nSembra che abbia i **Guanti** d'arme aperti, come se aspettasse qualcosa.";
    const char* nome_oggetto = "Guanti";
    const char* descrizione_oggetto = "Va inserito qualcosa nei guanti";
    const char* soluzione = "Alabarda";
    const char* nuovo_oggetto = "Clessidra_Grande";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 0; // Non apre porte
    l->items[0].location_index = -1; 

    l->items[0].bonus_time = 60; // Tempo bonus di 1 minuto
    l->items[0].bonus_points = 0;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 2;

    l->items[0].enigma.question = (char*) malloc((strlen(nuovo_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, nuovo_oggetto);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzione) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzione);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location teca
int create_teca(struct location* l, const int index){
    const char* nome_location = "Teca";
    const char* descrizione_location = "L'uomo raffigurato ha un aspetto austero. Sembra quasi essere un soldato, un inquisitore o comunque qualcuno con un ruolo militare. Trasali. Percepisci come se quest'uomo avesse fatto qualcosa di estremamente sbagliato. Ti giri e te ne vai. \nQua non trovi nulla di che.";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Nessun oggetto   
    l->num_items = 0;
    l->items = NULL;

    return 0;
}

// Crea la location comodino
int create_comodino(struct location* l, const int index){
    int indice, i = 0, j;
    int numero = 4; // Numero degli indovinelli

    const char* nome_location = "Comodino";
    const char* descrizione_location = "Sopra questo comodino si trova una strana **Clessidra**.";
    const char* nome_oggetto = "Clessidra";
    const char* descrizione_oggetto = "Questa clessidra sembra avere qualche strano potere. Per prenderla devi risolvere un enigma:";

    const char* indovinelli[] = {
        "Quanto è lungo il lato di un cubo se il suo volume è di 27 metri cubi?",
        "Quante lettere ci sono nell'alfabeto?",
        "Qual è la cosa che tutti fanno allo stesso modo?",
        "Cosa può essere rotto, ma non può essere toccato?",
    };

    const char* soluzioni[] = {
        "3 metri", // Quanto è lungo il lato di un cubo se il suo volume è di 27 metri cubi?
        "26", // Quante lettere ci sono nell'alfabeto?
        "Invecchiare", // Qual è la cosa che tutti fanno allo stesso modo?
        "Promessa", // Cosa può essere rotto, ma non può essere toccato?
    };

    const char* errata1[] = {
        "4 metri", // Quanto è lungo il lato di un cubo se il suo volume è di 27 metri cubi?
        "20", // Quante lettere ci sono nell'alfabeto?
        "Respirare", // Qual è la cosa che tutti fanno allo stesso modo?
        "Segreto", // Cosa può essere rotto, ma non può essere toccato?
    };

    const char* errata2[] = {
        "2 metri", // Quanto è lungo il lato di un cubo se il suo volume è di 27 metri cubi?
        "11", // Quante lettere ci sono nell'alfabeto?
        "Dormire", // Qual è la cosa che tutti fanno allo stesso modo?
        "Sogno", // Cosa può essere rotto, ma non può essere toccato?
    };

    const char* errata3[] = {
        "5 metri", // Quanto è lungo il lato di un cubo se il suo volume è di 27 metri cubi?
        "30", // Quante lettere ci sono nell'alfabeto?
        "Mangiare", // Qual è la cosa che tutti fanno allo stesso modo?
        "Emozione", // Cosa può essere rotto, ma non può essere toccato?
    };

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index;
    l->linked_locations[0].is_blocked = 0;

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 1;

    l->items[0].opens_door = 0;
    l->items[0].location_index = -1;

    l->items[0].bonus_time = 15;
    l->items[0].bonus_points = 0;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    indice = my_random(0, numero - 1);
    l->items[0].enigma.type = 1;

    // Creazione delle opzioni
    l->items[0].enigma.question = (char*) malloc((strlen(indovinelli[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, indovinelli[indice]);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzioni[indice]);

    l->items[0].enigma.attempts_left = 1;

    l->items[0].enigma.options[0] = (char*) malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[0] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[0], soluzioni[indice]);

    l->items[0].enigma.options[1] = (char*) malloc((strlen(errata1[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[1] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[1], errata1[indice]);

    l->items[0].enigma.options[2] = (char*) malloc((strlen(errata2[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[2] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[2], errata2[indice]);

    l->items[0].enigma.options[3] = (char*) malloc((strlen(errata3[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[3] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[3], errata3[indice]);
        
    // Mescola le opzioni
    for(; i < 3; i++){
        j = my_random(3, i);
        swap_string(&(l->items[0].enigma.options[i]), &(l->items[0].enigma.options[j]));
    }

    return 0;
}

// Funzione per creare la biblioteca nella escape room medievale
int create_medieval_library_location(struct escape_room_descriptor* r, const int num_rooms, const int index){
    int ret;
    const char* nome_location = "Biblioteca";
    const char* descrizione_location = "Entri in una vasta sala del Castello con scaffali pieni di antichi tomi e pergamene, illuminati da fioca luce proveniente da grandi finestre. \nTra i vari testi, si nota un libro antico posizionato su un ++Tavolo Centrale++, uno ++Scaffale++ in cui manca un libro e infine una ++Finestra++. \nVi è anche una porta che vi porterà a ++???++ e la porta che vi riporterà alla location precedente (visualizzabili con il comando doors). ";

    // Biblioteca [index]
    r->rooms[index].name = (char*)malloc((strlen(nome_location) + 1) * sizeof(char));
    if(r->rooms[index].name == NULL)
        return -1;
    strcpy(r->rooms[index].name, nome_location);

    r->rooms[index].description = (char*)malloc((strlen(descrizione_location ) + 1) * sizeof(char));
    if(r->rooms[index].description == NULL)
        return -1;
    strcpy(r->rooms[index].description, descrizione_location );
    
    r->rooms[index].num_items = 0;
    r->rooms[index].items = NULL;
    
     // Ti colleghi a 5 location
    r->rooms[index].num_doors = 5;
    r->rooms[index].linked_locations = (struct door*) malloc(r->rooms->num_doors  * sizeof(struct door));
    if(r->rooms[index].linked_locations  == NULL)
        return -1;

    // Quella successiva. 
    r->rooms[index].linked_locations[0].next_location = index+1;
    r->rooms[index].linked_locations[0].is_blocked = 1;

    // Quella precedente. La stanza dei quadri è particolare e quindi viene lasciata aperta. Se no è chiusa. 
    r->rooms[index].linked_locations[1].next_location = index-1;
    if(strcmp(r->rooms[index-1].name, "Stanza dei Quadri") == 0)
        r->rooms[index].linked_locations[1].is_blocked = 0;
    else
        r->rooms[index].linked_locations[1].is_blocked = 1;
    
    // Collegamenti alle location interne
    r->rooms[index].linked_locations[2].next_location = num_rooms + index * 3;
    r->rooms[index].linked_locations[2].is_blocked = 0;
    r->rooms[index].linked_locations[3].next_location = num_rooms + index * 3 + 1;
    r->rooms[index].linked_locations[3].is_blocked = 0;
    r->rooms[index].linked_locations[4].next_location = num_rooms + index * 3 + 2;
    r->rooms[index].linked_locations[4].is_blocked = 0;

    r->rooms[index].is_final = 0; // Non è la stanza finale
    r->rooms[index].points_needed = 0; // Non servono punti per interagire


    // Tavolo Centrale [num_rooms + index * 3]
    ret = create_tavolo_centrale(&r->rooms[num_rooms + index * 3], index);
    if(ret < 0)
        return ret;

    // Scaffale [num_rooms + index * 3 + 1]
    ret = create_scaffale(&r->rooms[num_rooms + index * 3 + 1], index);
    if(ret < 0)
        return ret;

    // Finestra [num_rooms + index * 3 + 2]
    ret = create_finestra(&r->rooms[num_rooms + index * 3 + 2], index);
    if(ret < 0)
        return ret;

    return 0;
}

// Crea la location Tavolo Centrale
int create_tavolo_centrale(struct location* l, const int index){
    int indice, i = 0;
    int numero = 3;

    const char* nome_location = "Tavolo Centrale";
    const char* descrizione_location = "In questo tavolo, si trova un grande e polveroso **Libro_Antico**.";
    const char* nome_oggetto = "Libro_Antico";
    const char* descrizione_oggetto = "Un antico libro dall'aspetto misterioso, posato su un tavolo. Potrebbe contenere segreti o informazioni utili.";
                                    

    const char* indovinelli[] = {
        "Attraverso l'aria viaggia senza peso, senza forma, né colore, è un mistero a ogni sguardo. Può essere dolce, oppure tuonare con furore, è invisibile, ma con potenza si fa amare.",
        "Qual è la cosa che è più facile rompere quando si dice il nome?",
        "Riesci a indovinare: ha un occhio ma non vede?"
    };

    const char* soluzioni[] = {
        "Suono", // Cosa è che non ha vita, ma può morire?
        "Silenzio", // Qual è la cosa che è più facile rompere quando si dice il nome?
        "Ago" // Riesci a indovinare: ha un occhio ma non vede?
    };

    // Generazione del Libro Antico
    l->name = (char*)malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name == NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*)malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    l->num_items = 1;
    l->items = (struct object*)malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*)malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*)malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 0;
    l->items[0].location_index = -1;

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 0;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 0;

    indice = my_random(0, numero - 1);
    l->items[0].enigma.question = (char*)malloc((strlen(indovinelli[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.question == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, indovinelli[indice]);

    l->items[0].enigma.solution = (char*)malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.solution == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzioni[indice]);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location Scaffale
int create_scaffale(struct location* l, const int index){
    int i = 0;

    const char* nome_location = "Scaffale";
    const char* descrizione_location = "In questo scaffale pieno di libri c'è uno **Spazio_Vuoto**. Sembra manchi qualcosa. Cosa potrebbe entrare in uno spazio vuoto in uno scaffale pieno di libri?";
    const char* nome_oggetto = "Spazio_Vuoto";
    const char* descrizione_oggetto = "Va inserito qualcosa in questo spazio vuoto";
    const char* soluzione = "Libro_Antico";
    const char* nuovo_oggetto = "Chiave";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 1; // Apre una porta
    l->items[0].location_index = index+1; // Location successiva rispetto alla stanza in cui si trova questa location

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 1;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 2;

    l->items[0].enigma.question = (char*) malloc((strlen(nuovo_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, nuovo_oggetto);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzione) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzione);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location Finestra
int create_finestra(struct location* l, const int index){
    const char* nome_location = "Finestra";
    const char* descrizione_location = "Dalla finestra vedi una montagna in lontanza. Questo Castello è molto alto e ti domandi a chi potesse appartenere un Castello così imponente. Sicuramente chiunque lo possedesse doveva essere una persona molto potente, appartenente ad una famiglia molto ricca. Eppure non hai ancora visto uno stendardo. \nQua non trovi nulla di che.";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index;
    l->linked_locations[0].is_blocked = 0;

    // Nessun oggetto   
    l->num_items = 0;
    l->items = NULL;

    return 0;
}

// Crea la location della stanza della chiesa per la escape room medievale 
int create_medieval_church_location(struct escape_room_descriptor* r, const int num_rooms, const int index){
    int ret;
    const char* nome_location = "Chiesa";
    const char* descrizione_location = "Questa Chiesa sembra essere abbandonata da molto, nonostante ciò è tutto sommato in un buono stato. Regna un silenzio tombale. La luce entra da un rosone e illumina bene la stanza. Sembra essere una piccola cappella all'interno del Castello. \nQua dentro si trovano un ++Altare**, un'++Acqua Santiera++ e un ++Piedistallo++. \nVi è anche una porta che vi porterà a ++???++ e la porta che vi riporterà alla location precedente (visualizzabili con il comando doors).";

    // Biblioteca [index]
    r->rooms[index].name = (char*)malloc((strlen(nome_location) + 1) * sizeof(char));
    if(r->rooms[index].name == NULL)
        return -1;
    strcpy(r->rooms[index].name, nome_location);

    r->rooms[index].description = (char*)malloc((strlen(descrizione_location ) + 1) * sizeof(char));
    if(r->rooms[index].description == NULL)
        return -1;
    strcpy(r->rooms[index].description, descrizione_location );
    
    r->rooms[index].num_items = 0;
    r->rooms[index].items = NULL;
    
    // Ti colleghi a 5 location
    r->rooms[index].num_doors = 5;
    r->rooms[index].linked_locations = (struct door*) malloc(r->rooms->num_doors  * sizeof(struct door));
    if(r->rooms[index].linked_locations  == NULL)
        return -1;

    // Quella successiva
    r->rooms[index].linked_locations[0].next_location = index+1;
    r->rooms[index].linked_locations[0].is_blocked = 1;

    // Quella precedente. La stanza dei quadri è particolare e quindi viene lasciata aperta. Se no è chiusa. 
    r->rooms[index].linked_locations[1].next_location = index-1;
    if(strcmp(r->rooms[index-1].name, "Stanza dei Quadri") == 0)
        r->rooms[index].linked_locations[1].is_blocked = 0;
    else
        r->rooms[index].linked_locations[1].is_blocked = 1;
    
    // Collegamenti alle location interne
    r->rooms[index].linked_locations[2].next_location = num_rooms + index * 3;
    r->rooms[index].linked_locations[2].is_blocked = 0;
    r->rooms[index].linked_locations[3].next_location = num_rooms + index * 3 + 1;
    r->rooms[index].linked_locations[3].is_blocked = 0;
    r->rooms[index].linked_locations[4].next_location = num_rooms + index * 3 + 2;
    r->rooms[index].linked_locations[4].is_blocked = 0;

    r->rooms[index].is_final = 0; // Non è la stanza finale
    r->rooms[index].points_needed = 0; // Non servono punti per interagire

    // Altare [num_rooms + index * 3]
    ret = create_altare(&r->rooms[num_rooms + index * 3], index);
    if(ret < 0)
        return ret;

    // Acqua Santiera [num_rooms + index * 3 + 1]
    ret = create_acqua_santiera(&r->rooms[num_rooms + index * 3 + 1], index);
    if(ret < 0)
        return ret;

    // Piedistallo [num_rooms + index * 3 + 2]
    ret = create_piedistallo(&r->rooms[num_rooms + index * 3 + 2], index);
    if(ret < 0)
        return ret;

    return 0;
}

// Crea la location Altare
int create_altare(struct location* l, const int index){
    int indice, i = 0, j;
    int numero = 5; // Numero di indovinelli

    const char* nome_location = "Altare";
    const char* descrizione_location = "In questo Altare si trova una **Croce**.";
    const char* nome_oggetto = "Croce";
    const char* descrizione_oggetto = "Questa croce polverosa è di un'ottima fattura. Magari serve per qualcosa.";
                                    

    const char* indovinelli[] = {
        "Qual è la cosa che Pietro fece tre volte di fronte alla crocifissione di Gesù?",
        "Cosa è che un uomo trovò sul ciglio della strada e curò, portandolo a una locanda?",
        "Qual è il dono dello Spirito Santo ricevuto dagli apostoli nel giorno di Pentecoste?",
        "Chi fu il primo martire cristiano menzionato nel Nuovo Testamento?",
        "Cosa ricevettero gli apostoli nel giorno di Pentecoste secondo il libro degli Atti?"
    };

    const char* soluzioni[] = {
        "Rinnegò Gesù", // Qual è la cosa che Pietro fece tre volte di fronte alla crocifissione di Gesù?
        "Uomo mezzo morto", // Cosa è che un uomo trovò sul ciglio della strada e curò, portandolo a una locanda?
        "Lingue di fuoco", // Qual è il dono dello Spirito Santo ricevuto dagli apostoli nel giorno di Pentecoste?
        "Stefano", // Chi fu il primo martire cristiano menzionato nel Nuovo Testamento?
        "Spirito Santo" // Cosa ricevettero gli apostoli nel giorno di Pentecoste secondo il libro degli Atti?
    };

    const char* errata1[] = {
        "Pregò", // Qual è la cosa che Pietro fece tre volte di fronte alla crocifissione di Gesù?
        "Mendicante", // Cosa è che un uomo trovò sul ciglio della strada e curò, portandolo a una locanda?
        "Corone di fiori", // Qual è il dono dello Spirito Santo ricevuto dagli apostoli nel giorno di Pentecoste?
        "Matteo", // Chi fu il primo martire cristiano menzionato nel Nuovo Testamento?
        "Pane" // Cosa ricevettero gli apostoli nel giorno di Pentecoste secondo il libro degli Atti?
    };

    const char* errata2[] = {
        "Piangé", // Qual è la cosa che Pietro fece tre volte di fronte alla crocifissione di Gesù?
        "Estraneo", // Cosa è che un uomo trovò sul ciglio della strada e curò, portandolo a una locanda?
        "Ali di farfalla", // Qual è il dono dello Spirito Santo ricevuto dagli apostoli nel giorno di Pentecoste?
        "Paolo", // Chi fu il primo martire cristiano menzionato nel Nuovo Testamento?
        "Segno celeste" // Cosa ricevettero gli apostoli nel giorno di Pentecoste secondo il libro degli Atti?
    };

    const char* errata3[] = {
        "Aiutò Maria", // Qual è la cosa che Pietro fece tre volte di fronte alla crocifissione di Gesù?
        "Mendicante cieco", // Cosa è che un uomo trovò sul ciglio della strada e curò, portandolo a una locanda?
        "Frecce di luce", // Qual è il dono dello Spirito Santo ricevuto dagli apostoli nel giorno di Pentecoste?
        "Giovanni", // Chi fu il primo martire cristiano menzionato nel Nuovo Testamento?
        "Visione" // Cosa ricevettero gli apostoli nel giorno di Pentecoste secondo il libro degli Atti?
    };

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 1;

    l->items[0].opens_door = 0;
    l->items[0].location_index = -1;

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 0;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    indice = my_random(0, numero - 1);
    l->items[0].enigma.type = 1;

    // Creazione delle opzioni
    l->items[0].enigma.question = (char*) malloc((strlen(indovinelli[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, indovinelli[indice]);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzioni[indice]);

    l->items[0].enigma.attempts_left = 3;

    l->items[0].enigma.options[0] = (char*) malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[0] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[0], soluzioni[indice]);

    l->items[0].enigma.options[1] = (char*) malloc((strlen(errata1[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[1] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[1], errata1[indice]);

    l->items[0].enigma.options[2] = (char*) malloc((strlen(errata2[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[2] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[2], errata2[indice]);

    l->items[0].enigma.options[3] = (char*) malloc((strlen(errata3[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.options[3] == NULL)
        return -1;
    strcpy(l->items[0].enigma.options[3], errata3[indice]);
        
    // Mescola le opzioni
    for(; i < 3; i++){
        j = my_random(3, i);
        swap_string(&(l->items[0].enigma.options[i]), &(l->items[0].enigma.options[j]));
    }

    return 0;
}

// Crea la location Acqua Santiera
int create_acqua_santiera(struct location* l, const int index){
    int i = 0;

    const char* nome_location = "Acqua Santiera";
    const char* descrizione_location = "In questa Acqua Santiera è possibile inserire qualcosa nel **Contenitore**.";
    const char* nome_oggetto = "Contenitore";
    const char* descrizione_oggetto = "Va inserito qualcosa in questo contenitore";
    const char* soluzione = "Caraffa";
    const char* nuovo_oggetto = "Chiave";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 1; // Apre una porta
    l->items[0].location_index = index+1; // Quella successiva 

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 1;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 2;

    l->items[0].enigma.question = (char*) malloc((strlen(nuovo_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, nuovo_oggetto);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzione) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzione);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location Piedistallo
int create_piedistallo(struct location* l, const int index){
    int i = 0;

    const char* nome_location = "Piedistallo";
    const char* descrizione_location = "Su questo Piedistallo si trova una **Caraffa**. Su questo piedistallo c'è un incavo a forma di croce.";
    const char* nome_oggetto = "Caraffa";
    const char* descrizione_oggetto = "Questa caraffa è piena d'acqua, ma è bloccata.";
    const char* soluzione = "Croce";
    const char* nuovo_oggetto = "Caraffa";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 0;
    l->items[0].location_index = -1;

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 0;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 2;

    l->items[0].enigma.question = (char*) malloc((strlen(nuovo_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, nuovo_oggetto);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzione) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzione);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location della stanza da pranzo per la escape room medievale 
int create_medieval_dining_location(struct escape_room_descriptor* r, const int num_rooms, const int index){
    int ret;
    const char* nome_location = "Sala da Pranzo";
    const char* descrizione_location = "In questa stanza un tempo veniva usati per i pranzi in famiglia. Devo dire che è molto grande... \nQua dentro si trova un ++Tavolo da Pranzo++, una ++Finestra++ e un'++Enoteca++. \nVi è anche una porta che vi porterà a ++???++ e la porta che vi riporterà alla location precedente (visualizzabili con il comando doors).";

    // Stanza delle Torture [index]
    r->rooms[index].name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(r->rooms[0].name== NULL)
        return -1;
    strcpy(r->rooms[index].name, nome_location);

    r->rooms[index].description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(r->rooms[index].description == NULL)
        return -1;

    strcpy(r->rooms[index].description, descrizione_location);

    r->rooms[index].num_items = 0;
    r->rooms[index].items = NULL;
    
    // Ti colleghi a 5 location
    r->rooms[index].num_doors = 5;
    r->rooms[index].linked_locations = (struct door*) malloc(r->rooms->num_doors  * sizeof(struct door));
    if(r->rooms[index].linked_locations  == NULL)
        return -1;

    // Quella successiva
    r->rooms[index].linked_locations[0].next_location = index+1;
    r->rooms[index].linked_locations[0].is_blocked = 1;

    // Quella precedente. La stanza dei quadri è particolare e quindi viene lasciata aperta. Se no è chiusa. 
    r->rooms[index].linked_locations[1].next_location = index-1;
    if(strcmp(r->rooms[index-1].name, "Stanza dei Quadri") == 0)
        r->rooms[index].linked_locations[1].is_blocked = 0;
    else
        r->rooms[index].linked_locations[1].is_blocked = 1;
    
    // Collegamenti alle location interne
    r->rooms[index].linked_locations[2].next_location = num_rooms + index * 3;
    r->rooms[index].linked_locations[2].is_blocked = 0;
    r->rooms[index].linked_locations[3].next_location = num_rooms + index * 3 + 1;
    r->rooms[index].linked_locations[3].is_blocked = 0;
    r->rooms[index].linked_locations[4].next_location = num_rooms + index * 3 + 2;
    r->rooms[index].linked_locations[4].is_blocked = 0;

    r->rooms[index].is_final = 0;
    r->rooms[index].points_needed = 0;
    
    //  [num_rooms + index * 3]
    ret = create_tavolo_pranzo(&r->rooms[num_rooms + index * 3], index);
    if(ret < 0)
        return ret;

    // Finestra [num_rooms + index * 3 + 1]
    ret = create_finestra(&r->rooms[num_rooms + index * 3 + 1], index);
    if(ret < 0)
        return ret;

    //  [num_rooms + index * 3 + 2]
    ret = create_enoteca(&r->rooms[num_rooms + index * 3 + 2], index);
    if(ret < 0)
        return ret;

    return 0;
}

// Crea la location Tavolo da Pranzo
int create_tavolo_pranzo(struct location* l, const int index){
    int i = 0;

    const char* nome_location = "Tavolo da Pranzo";
    const char* descrizione_location = "Questo tavolo è apparecchiato, ma non sembra esserci nulla di rilevante ... se non un **Calice**.";
    const char* nome_oggetto = "Calice";
    const char* descrizione_oggetto = "Va inserito qualcosa in questo calice";
    const char* soluzione = "Bottiglia";
    const char* nuovo_oggetto = "Chiave";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 1; // Apre una porta
    l->items[0].location_index = index+1; // Quella successiva

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 1;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 2;

    l->items[0].enigma.question = (char*) malloc((strlen(nuovo_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, nuovo_oggetto);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzione) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzione);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location Enoteca
int create_enoteca(struct location* l, const int index){
    int indice, i = 0;
    int numero = 4;

    const char* nome_location = "Enoteca";
    const char* descrizione_location = "Questa Enoteca contiene un sacco di bottiglie vuote e inutili. Tuttavia sembra esserci una **Bottiglia** di vino piena.";
    const char* nome_oggetto = "Bottiglia";
    const char* descrizione_oggetto = "Una bottiglia piena di vino.";
                                    

    const char* indovinelli[] = {
        "Cos'è che rende il cuore allegro, ma non è vivo?",
        "Cosa si può trovare in bottiglia, ma non è mai stata imbottigliato?",
        "Chi è che si trasforma con il tempo, ma non invecchia?",
        "Cos'è che può essere rotto, ma mai ferito?"
    };

    const char* soluzioni[] = {
        "Vino", // Cos'è che rende il cuore allegro, ma non è vivo?
        "Aroma", // Cosa si può trovare in bottiglia, ma non è mai stata imbottigliato?
        "Mosto", // Chi è che si trasforma con il tempo, ma non invecchia?
        "Bicchiere", // Cos'è che può essere rotto, ma mai ferito?
    };

    // Generazione del Libro Antico
    l->name = (char*)malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name == NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*)malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata
    
    l->num_items = 1;
    l->items = (struct object*)malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*)malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*)malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 0;
    l->items[0].location_index = -1;

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 0;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 0;

    indice = my_random(0, numero - 1);
    l->items[0].enigma.question = (char*)malloc((strlen(indovinelli[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.question == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, indovinelli[indice]);

    l->items[0].enigma.solution = (char*)malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.solution == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzioni[indice]);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea l'ultima location per la escape room medievale 
int create_medieval_last_location(struct escape_room_descriptor* r, const int num_rooms, const int index){
    int ret, i = 0;
    const char* nome_location = "Stanza Finale";
    const char* descrizione_location = "Questa stanza sembra essere una forgia. Davanti a me si trova l'**Uscita**, ma non è possibile attraversarla. Serve una chiave. \nDavanti a te c'è una ++Forgia++, uno ++Scrigno++ e infine una ++Stanza Chiavi++. \nPuoi tornare indietro attraverso la porta.";
    const char* nome_oggetto = "Uscita";
    const char* descrizione_oggetto = "Serve una chiave";
    const char* soluzione = "Chiave_Finale";
    const char* nuovo_oggetto = "Uscita";

    // Biblioteca [index]
    r->rooms[index].name = (char*)malloc((strlen(nome_location) + 1) * sizeof(char));
    if(r->rooms[index].name == NULL)
        return -1;
    strcpy(r->rooms[index].name, nome_location);

    r->rooms[index].description = (char*)malloc((strlen(descrizione_location ) + 1) * sizeof(char));
    if(r->rooms[index].description == NULL)
        return -1;
    strcpy(r->rooms[index].description, descrizione_location );
    
    // Ti colleghi a 4 location
    r->rooms[index].num_doors = 4;
    r->rooms[index].linked_locations = (struct door*) malloc(r->rooms->num_doors  * sizeof(struct door));
    if(r->rooms[index].linked_locations  == NULL)
        return -1;

    // Quella precedente. La stanza dei quadri è particolare e quindi viene lasciata aperta. Se no è chiusa. 
    r->rooms[index].linked_locations[0].next_location = index-1;
    if(strcmp(r->rooms[index-1].name, "Stanza dei Quadri") == 0)
        r->rooms[index].linked_locations[0].is_blocked = 0;
    else
        r->rooms[index].linked_locations[0].is_blocked = 1;
    
     // Collegamenti alle location interne
    r->rooms[index].linked_locations[1].next_location = num_rooms + index * 3;
    r->rooms[index].linked_locations[1].is_blocked = 0;
    r->rooms[index].linked_locations[2].next_location = num_rooms + index * 3 + 1;
    r->rooms[index].linked_locations[2].is_blocked = 0;
    r->rooms[index].linked_locations[3].next_location = num_rooms + index * 3 + 2;
    r->rooms[index].linked_locations[3].is_blocked = 0;

    r->rooms[index].is_final = 1; // Questa è la stanza finale
    r->rooms[index].points_needed = 1; // C'è bisogno di almeno un punto per interagire

    // Generazione oggetto   
    r->rooms[index].num_items = 1;
    r->rooms[index].items = (struct object*) malloc(r->rooms[index].num_items * sizeof(struct object));
    if(r->rooms[index].items == NULL)
        return -1;

    r->rooms[index].items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(r->rooms[index].items[0].name == NULL)
        return -1;
    strcpy(r->rooms[index].items[0].name, nome_oggetto);

    r->rooms[index].items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(r->rooms[index].items[0].description== NULL)
        return -1;
    strcpy(r->rooms[index].items[0].description, descrizione_oggetto);

    r->rooms[index].items[0].type = 3; // Tipo speciale
    // L'oggetto serve. Il fatto che la stanza sia quella finale tramite "is_final" e di tipo 3, significa che l'oggetto permette di fuggire. 

    r->rooms[index].items[0].opens_door = 0; // Non apre porte perché è l'uscita
    r->rooms[index].items[0].location_index = -1;

    r->rooms[index].items[0].bonus_time = 0;
    r->rooms[index].items[0].bonus_points = 1;
    r->rooms[index].items[0].usable = 1;
    r->rooms[index].items[0].player = -1;

    r->rooms[index].items[0].riddle_answer = 1;

    // Generazione enigma
    r->rooms[index].items[0].enigma.type = 2;

    r->rooms[index].items[0].enigma.question = (char*) malloc((strlen(nuovo_oggetto) + 1) * sizeof(char));
    if(r->rooms[index].items[0].description == NULL)
        return -1;
    strcpy(r->rooms[index].items[0].enigma.question, nuovo_oggetto);

    r->rooms[index].items[0].enigma.solution = (char*) malloc((strlen(soluzione) + 1) * sizeof(char));
    if(r->rooms[index].items[0].description == NULL)
        return -1;
    strcpy(r->rooms[index].items[0].enigma.solution, soluzione);

    r->rooms[index].items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        r->rooms[index].items[0].enigma.options[i] = NULL;

    // Forgia [num_rooms + index * 3]
    ret = create_forgia(&r->rooms[num_rooms + index * 3], index);
    if(ret < 0)
        return ret;

    // Scrigno[num_rooms + index * 3 + 1]
    ret = create_scrigno(&r->rooms[num_rooms + index * 3 + 1], index);
    if(ret < 0)
        return ret;

    // Stanza delle Chiavi [num_rooms + index * 3 + 2]
    ret = create_stanza_chiavi(&r->rooms[num_rooms + index * 3 + 2], index);
    if(ret < 0)
        return ret;

    return 0;
}

// Crea la location Forgia
int create_forgia(struct location* l, const int index){
    int i = 0;

    const char* nome_location = "Forgia";
    const char* descrizione_location = "Questa forgia è da pareccho in disuso, ma con un po' di tempo puoi riattivarla. Potresti usare l'**Incudine** per forgiare qualcosa ... ";
    const char* nome_oggetto = "Incudine";
    const char* descrizione_oggetto = "Ci puoi forgiare qualcosa";
    const char* soluzione = "Chiave_Rotta";
    const char* nuovo_oggetto = "Chiave_Finale";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 0;
    l->items[0].location_index = -1;

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 1;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 2;

    l->items[0].enigma.question = (char*) malloc((strlen(nuovo_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, nuovo_oggetto);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzione) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzione);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location Scrigno
int create_scrigno(struct location* l, const int index){
    int i = 0;

    const char* nome_location = "Scrigno";
    const char* descrizione_location = "Questo Scrigno potrebbe contenere qualcosa di importante. Devo infilare una chiave nella **Serratura**.";
    const char* nome_oggetto = "Serratura";
    const char* descrizione_oggetto = "Deve essere aperta con una chiave";
    const char* soluzione = "Chiave_Scrigno";
    const char* nuovo_oggetto = "Chiave_Rotta";

    // Generazione location
    l->name = (char*) malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name== NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*) malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    // Generazione oggetto   
    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*) malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*) malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description== NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 0;
    l->items[0].location_index = -1;

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 1;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 2;

    l->items[0].enigma.question = (char*) malloc((strlen(nuovo_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, nuovo_oggetto);

    l->items[0].enigma.solution = (char*) malloc((strlen(soluzione) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzione);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Crea la location Stanze delle Chiavi
int create_stanza_chiavi(struct location* l, const int index){
    int indice, i = 0;
    int numero = 4;

    const char* nome_location = "Stanza Chiavi";
    const char* descrizione_location = "In questa stanza cerchi varie chiavi per aprire l'uscita, ma non trovi nulla. Però una chiave ti sembra particolare. \nTrovi la **Chiave_Scrigno**.";
    const char* nome_oggetto = "Chiave_Scrigno";
    const char* descrizione_oggetto = "Questa chiave sembra che possa aprire lo scrigno.";
                                    
    const char* indovinelli[] = {
        "Cosa può modellare il ferro, ma non lo tocca?",
        "Cos'è che riscalda ma non si brucia?",
        "Chi è che crea senza parlare?",
        "Cosa viene plasmata e plasmata, ma non cambia mai forma?"
    };

    const char* soluzioni[] = {
        "Fucina", // Cosa può modellare il ferro, ma non lo tocca?
        "Aria", // Cos'è che riscalda ma non si brucia?
        "Fabbro", // Chi è che crea senza parlare?
        "Storia", // Cosa viene plasmata e plasmata, ma non cambia mai forma?
    };

    // Generazione del Libro Antico
    l->name = (char*)malloc((strlen(nome_location) + 1) * sizeof(char));
    if(l->name == NULL)
        return -1;
    strcpy(l->name, nome_location);

    l->description = (char*)malloc((strlen(descrizione_location) + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;
    strcpy(l->description, descrizione_location);

    l->is_final = 0;
    l->points_needed = 0;

    // Per tornare indietro
    l->num_doors = 1;
    l->linked_locations = (struct door*) malloc(l->num_doors * sizeof(struct door));
    if(l->linked_locations == NULL)
        return -1;
    l->linked_locations[0].next_location = index; // La locazione a cui appartiene
    l->linked_locations[0].is_blocked = 0; // Non è bloccata

    l->num_items = 1;
    l->items = (struct object*) malloc(l->num_items * sizeof(struct object));
    if(l->items == NULL)
        return -1;

    l->items[0].name = (char*)malloc((strlen(nome_oggetto) + 1) * sizeof(char));
    if(l->items[0].name == NULL)
        return -1;
    strcpy(l->items[0].name, nome_oggetto);

    l->items[0].description = (char*)malloc((strlen(descrizione_oggetto) + 1) * sizeof(char));
    if(l->items[0].description == NULL)
        return -1;
    strcpy(l->items[0].description, descrizione_oggetto);

    l->items[0].type = 0;

    l->items[0].opens_door = 0;
    l->items[0].location_index = -1;

    l->items[0].bonus_time = 0;
    l->items[0].bonus_points = 0;
    l->items[0].usable = 1;
    l->items[0].player = -1;

    l->items[0].riddle_answer = 1;

    // Generazione enigma
    l->items[0].enigma.type = 0;

    indice = my_random(0, numero - 1);
    l->items[0].enigma.question = (char*)malloc((strlen(indovinelli[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.question == NULL)
        return -1;
    strcpy(l->items[0].enigma.question, indovinelli[indice]);

    l->items[0].enigma.solution = (char*)malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
    if(l->items[0].enigma.solution == NULL)
        return -1;
    strcpy(l->items[0].enigma.solution, soluzioni[indice]);

    l->items[0].enigma.attempts_left = 1;

    for(; i < 4; i++)
        l->items[0].enigma.options[i] = NULL;

    return 0;
}

// Indice 1: Escape Room dell'Antico Egitto
int create_ancient_egypt_escape_room(struct escape_room_descriptor* r){
    int i = 0, j = 0, ret;
    const char* stanze[] = {
        "Camera Piramide 1",
        "Camera Piramide 2",
        "Camera Piramide 3",
        "Camera Piramide 4",
    };
    const char* centro = "Stanza Centrale";

    ret = initialize_ancient_egypt_escape_room(r); // Inizializza il tema, la descrizione e il tipo della Escape Room con quella egiziana
    if(ret < 0)
        return ret;
    
    r->num_rooms = 5;
    r->rooms = (struct location*) malloc(r->num_rooms * sizeof(struct location));
    if(r->rooms == NULL)
        return -1;

    for(i = 0; i < r->num_rooms; i++){
        // Crea due porte
        r->rooms[i].num_doors = 2;
        r->rooms[i].linked_locations = (struct door*) malloc(r->rooms->num_doors  * sizeof(struct door));
        if(r->rooms[i].linked_locations == NULL){
            r->num_rooms = i;
            destroy_escape_room(r);
            return -1;
        }
        
        // Se non è l'ultima stanza entra qui e crea una Camera della Piramide
        if(i != 4){
            r->rooms[i].points_needed = 0;
            r->rooms[i].name = (char*) malloc((strlen(stanze[i]) + 1) * sizeof(char));
            if(r->rooms[i].name == NULL){
                // Gestione errore di allocazione di memoria
                r->num_rooms = i;
                destroy_escape_room(r);
                return -1;
            }
            strcpy(r->rooms[i].name, stanze[i]);

            // Se è la prima stanza è collegata a quella finale e quella successiva
            if(i == 0){
                r->rooms[i].linked_locations[0].next_location = 4;
                r->rooms[i].linked_locations[0].is_blocked = 0;
                r->rooms[i].linked_locations[1].next_location = 1;
                r->rooms[i].linked_locations[1].is_blocked = 0;
            }
            // Se no è collegata alla camera precedente e quella successiva
            else{
                r->rooms[i].linked_locations[0].next_location = i-1;
                r->rooms[i].linked_locations[0].is_blocked = 0;
                r->rooms[i].linked_locations[1].next_location = i+1;
                r->rooms[i].linked_locations[1].is_blocked = 0;
            }

            // Ogni stanza ha 7 oggetti
            r->rooms[i].num_items = 7;
            r->rooms[i].items = (struct object*) malloc(r->rooms[i].num_items * sizeof(struct object));
            if(r->rooms[i].items == NULL){
                free(r->rooms[i].name);
                free(r->rooms[i].linked_locations);
                r->num_rooms = i;
                destroy_escape_room(r);
                return -1;
            }
            
            // 4 oggetti danno punti e hanno un enigma a risposta aperta
            for(j = 0; j < 4; j++){
                ret = create_ancient_egypt_object(&r->rooms[i].items[j], 0, 0);
                if(ret < 0){
                    free(r->rooms[i].name);
                    free(r->rooms[i].linked_locations);
                    free(r->rooms[i].items);
                    r->num_rooms = i;
                    destroy_escape_room(r);
                    return ret;
                }
            }
            
            // Crea un oggetto che dà punti a risposta chiusa
            ret = create_ancient_egypt_object(&r->rooms[i].items[4], 0, 1);
            if(ret < 0){
                free(r->rooms[i].name);
                free(r->rooms[i].linked_locations);
                free(r->rooms[i].items);
                r->num_rooms = i;
                destroy_escape_room(r);
                return ret;
            }
            
            // Questo oggetto può essere utile o inutile, può avere un enigma a risposta aperta o a risposta chiusa
            ret = create_ancient_egypt_object(&r->rooms[i].items[5], my_random(0, 1), my_random(0, 1));
            if(ret < 0){
                free(r->rooms[i].name);
                free(r->rooms[i].linked_locations);
                free(r->rooms[i].items);
                r->num_rooms = i;
                destroy_escape_room(r);
                return ret;
            }

            // Questo oggetto è una trappola e toglie punti
            ret = create_ancient_egypt_object(&r->rooms[i].items[6], 2, 0);
            if(ret < 0){
                free(r->rooms[i].name);
                free(r->rooms[i].linked_locations);
                free(r->rooms[i].items);
                r->num_rooms = i;
                destroy_escape_room(r);
                return ret;
            }

            // Serve per mescolare gli oggetti in modo che non abbiano un ordine standard e riconoscibile
            srand(time(NULL));
            shuffle_objects(r->rooms[i].items, r->rooms[i].num_items);
            
            r->rooms[i].is_final = 0;
        }
        // Altrimenti crea l'ultima stanza
        else{
            r->rooms[i].points_needed = 10;
            r->rooms[i].name = (char*) malloc((strlen(centro) + 1) * sizeof(char));
            if(r->rooms[i].name == NULL){
                r->num_rooms = i;
                free(r->rooms[i].linked_locations);
                destroy_escape_room(r);
                return -1;
            }
            strcpy(r->rooms[i].name, centro);

            r->rooms[i].linked_locations[0].next_location = 3;
            r->rooms[i].linked_locations[0].is_blocked = 0;
            r->rooms[i].linked_locations[1].next_location = 0;
            r->rooms[i].linked_locations[1].is_blocked = 0;

            // L'oggetto finale che pone un enigma e una volta risolto si può uscire
            r->rooms[i].num_items = 1;
            r->rooms[i].items = (struct object*) malloc(r->rooms[i].num_items * sizeof(struct object));
            if(r->rooms[i].items == NULL){
                free(r->rooms[i].name);
                free(r->rooms[i].linked_locations);
                r->num_rooms = i;
                destroy_escape_room(r);
                return -1;
            }

            // Crea l'ultima stanza
            ret = create_ancient_egypt_tomb(&r->rooms[i].items[0]);
            if(ret < 0){
                free(r->rooms[i].name);
                free(r->rooms[i].linked_locations);
                free(r->rooms[i].items);
                r->num_rooms = i;
                destroy_escape_room(r);
                return ret;
            }
            
            r->rooms[i].points_needed = 10; // Ci vogliono 10 punti per interagire con la location
            r->rooms[i].is_final = 1; // Questa è la location finale
        }

        // In ogni caso crea una descrizione alla stanza che mostra tutti gli oggetti che vi sono
        ret = create_ancient_egypt_description(&r->rooms[i]);
        if(ret < 0){
            free(r->rooms[i].name);
            free(r->rooms[i].linked_locations);
            free(r->rooms[i].items);
            r->num_rooms = i;
            destroy_escape_room(r);
            return ret;
        }
    }

    return 0;
}

// Inizializza la Escape Room dell'Antico Egitto
int initialize_ancient_egypt_escape_room(struct escape_room_descriptor* r){
    const char* tema = "Escape Room Antico Egitto";
    const char* descrizione = "Escape Room in una antica Piramide Egizia. Per fuggire dovrai risolvere un determinato numero di indovinelli."; 

    r->theme = (char*) malloc((strlen(tema) + 1)  * sizeof(char));
    if(r->theme== NULL){
        printf("Errore nella creazione della Escape Room dell'Antico Egitto con il codice \n");
        return -1;
    }
    strcpy(r->theme, tema);


    r->description = (char*) malloc((strlen(descrizione) + 1) * sizeof(char));
    if(r->description == NULL){
        printf("Errore nella creazione della Escape Room dell'Antico Egitto con il codice \n");
        return -1;
    }
    strcpy(r->description, descrizione);

    r->type = 1;

    return 0;
}

// Crea gli oggetti per la Escape Room dell'Antico Egizio
int create_ancient_egypt_object(struct object* item, int type, int riddle){
    int indice, ret; 

    const char* nomi_oggetti[] = {
        "Vaso",
        "Statua",
        "Pergamena",
        "Collana",
        "Amuleto",
        "Piccola_Sfinge",
        "Torcia",
        "Pettine",
        "Orologio_Solare",
        "Pendente",
        "Lancia",
        "Fresca_Acqua",
        "Pittura_Murale",
        "Tavoletta_Argilla",
        "Candelabro",
        "Tavoletta_Offerte",
        "Pettorale",
        "Velo",
        "Stele",
        "Strumento_Musicale",
        "Spada",
        "Anfora",
        "Clessidra_Antica",
        "Bussola_del_Deserto",
        "Frammento_Antico_Rotolo"
    };

    const char* descrizioni_oggetti[] = {
        "Antico vaso rituale decorato con geroglifici.",
        "Statua di un dio egizio con sfumature d'oro.",
        "Pergamena con iscrizioni misteriose.",
        "Collana di pietre preziose.",
        "Amuleto incantato con poteri sconosciuti.",
        "Piccola sfinge enigmatica con il corpo di leone e volto umano.",
        "Torcia per illuminare gli oscuri corridoi della piramide.",
        "Pettine antico usato per acconciature rituali.",
        "Orologio solare che segnava il passaggio del tempo.",
        "Pendente con simboli sacri.",
        "Lancia da guerra decorata con simboli sacri.",
        "Fresca acqua conservata in un recipiente speciale.",
        "Pittura murale raffigurante scene di vita quotidiana.",
        "Tavoletta di argilla con iscrizioni cuneiformi.",
        "Candelabro usato per cerimonie rituali.",
        "Tavoletta delle offerte con raffigurazioni sacre.",
        "Pettorale adornato con simboli divini.",
        "Velo che copre un oggetto sacro.",
        "Stele con iscrizioni commemorative.",
        "Strumento musicale tradizionale egizio.",
        "Spada decorata con simboli antichi.",
        "Antica anfora usata per riti sacri.",
        "Clessidra antichissima dai poteri magici.",
        "Bussola per orientarsi nel deserto",
        "Frammento di antico rotolo con scritture indecifrabili."
    };

    const char* nomi_trappole[] = {
        "Pozione",
        "Scettro",
        "Anello",
        "Pietra_Preziosa",
        "Stoffa",
        "Fascio_di_Papiro",
        "Tavoletta_d'Oro",
        "Perla_del_Nilo",
        "Lente_del_Destino",
        "Moneta_Aldilà",
        "Occhio_di_Ra",
        "Frammento_Pietra_Solare",
        "Maschera_Funeraria",
        "Scatola_d'Incenso",
        "Tavoletta_dell'Oracolo",
        "Spilla_Ornamentale",
        "Sfera_d'Oro",
        "Calice_Sacro",
        "Scrigno_Dorato"
    };

    const char* descrizioni_trappole[] = {
        "Pozione magica dal potere misterioso.",
        "Scettro di potere decorato con gemme.",
        "Anello antico con incisioni misteriose.",
        "Pietra preziosa incastonata in un oggetto sacro.",
        "Stoffa pregiata con ricami dorati.",
        "Fascio di papiro con testi antichi scritti.",
        "Tavoletta d'oro incisa con simboli sacri.",
        "Perla preziosa proveniente dal fiume Nilo.",
        "Lente usata per scrutare il destino.",
        "Moneta leggendaria utilizzata nell'aldilà.",
        "Simbolo sacro dell'occhio divino di Ra.",
        "Frammento di pietra legato al sole e al potere divino.",
        "Maschera funeraria associata a riti sacri.",
        "Scatola contenente profumato incenso rituale.",
        "Tavoletta con profezie dell'oracolo antico.",
        "Spilla ornamentale associata a nobili antichi.",
        "Sfera d'oro custodita come sacra.",
        "Calice utilizzato in cerimonie religiose.",
        "Scrigno dorato di inestimabile valore."
    };  

    if(item == NULL)
        return -1;

    // Il tipo non può essere minore di 0 o maggiore di 2, quindi può essere chiave, inutile, o trappola
    if(type < 0 || type > 2)
        type = 1;
    
    item->type = type;
    item->opens_door = 0; // Non aprono porte
    item->location_index = -1;

    // Crea un oggetto chiave o inutile
    if(type <= 1){
        indice = my_random(0, 24);
        
        item->name = (char*) malloc((strlen(nomi_oggetti[indice])) * sizeof(char)); 
        if(item->name == NULL)
            return -1;
        strcpy(item->name, nomi_oggetti[indice]);

        item->description = (char*) malloc((strlen(descrizioni_oggetti[indice])) * sizeof(char)); 
        if(item->description == NULL){
            free(item->name);
            return -1;
        }
        strcpy(item->description, descrizioni_oggetti[indice]);
        
        item->usable = 0;
        item->player = -1;
        item->bonus_time = 0;
        
        // Se il tipo dell'indovinello è 0 o 1, ha un indovinello
        if(riddle == 0 || riddle == 1){
            item->riddle_answer = 1;
            ret = create_ancient_egypt_riddle(&item->enigma, riddle);
            if(ret < 0){
                free(item->name);
                free(item->description);
                return ret;
            }
        }
        // Se no no
        else
            item->riddle_answer = 0;

        // Se è chiave dà un punto
        if(type == 0)
            item->bonus_points = 1;
        // Altrimenti non dà punti
        else if(type == 1)
            item->bonus_points = 0;
    }
    // Crea un oggetto trappola
    else if(type == 2){
        indice = my_random(0, 18);

        item->name = (char*) malloc((strlen(nomi_trappole[indice])) * sizeof(char)); 
        if(item->name == NULL)
            return -1;
        strcpy(item->name, nomi_trappole[indice]);

        item->description = (char*) malloc((strlen(descrizioni_trappole[indice])) * sizeof(char)); 
        if(item->description == NULL){
            free(item->name);
            return -1;
        }
        strcpy(item->description, descrizioni_trappole[indice]);

        if(indice == 0 || indice == 12 || indice == 17){
            item->usable = 1;
            item->bonus_time = -60;
            item->bonus_points = 0;
        }
        else{
            item->usable = 0;
            item->bonus_time = 0;
            item->bonus_points = -1;
        }

        item->riddle_answer = 0; // Non deve rispondere ad indovinelli
    }

    return 0;
}

// Crea gli indovinelli per la Escape Room dell'Antico Egizio
int create_ancient_egypt_riddle(struct riddle* enigma, int type){
    int indice, i = 0, j = 0;
    int numero0 = 42;
    int numero1 = 10;

    const char *enigmi[] = {
        "Quante punte ha la piramide?",
        "Chi era il dio egizio del sole?",
        "Quale era l'animale sacro per gli antichi egizi?",
        "Qual è il nome del fiume che scorre attraverso l'antico Egitto?",
        "Quanti dei egizi sono presenti alla creazione?",
        "Qual è l'antico sistema di scrittura egizio?",
        "Qual era la funzione principale delle piramidi?",
        "Qual è il nome della città sacra degli antichi egizi?",
        "Qual era la principale divinità femminile egizia?",
        "Cosa significa la parola 'Ka'?",
        "Cosa rappresenta l'obelisco nell'antico Egitto?",
        "Chi era considerato il dio della luna?",
        "Chi era la dea della magia?",
        "Sono gli antichi sovrani dell'Antico Egitto, spesso sepolti in imponenti tombe chiamate valle dei re. Chi sono?",
        "Quali sono i giganteschi monumenti con teste umane e corpi di leone, poste all'ingresso di templi e tombe?",
        "Quali insetti sono il simbolo della resurrezione e della rinascita nell'Antico Egitto?",
        "Sono gli spiriti benevoli che assistono i defunti nel loro viaggio nell'aldilà. Chi sono?", 
        "Cosa è un piccolo idolo di argilla o legno usato come amuleto per garantire fortuna e protezione?",
        "I bastoni ricurvi utilizzati dagli antichi egizi come simbolo di autorità e potere.",
        "Complesso testo sacro e rituale dell'Antico Egitto, utilizzato nei riti religiosi e funerari.", 
        "Sono i fiori simbolici dell'Antico Egitto, spesso associati a rinascita e fertilità.",
        "Nasce al mattino, muore alla sera, è il simbolo della vita e dell'eternità.",
        "Molti cercano il mio segreto, sono un simbolo di saggezza e misticismo, la mia testa è adornata di piume.",
        "Misuro il tempo con la sabbia, segno il passaggio delle ere.",
        "Mi trovi nel Nilo, ho una pelle dura e proteggo i tesori nascosti.",
        "Indosso un copricapo bianco e ho un corno d'abbondanza, simbolo di fertilità e prosperità.",
        "Sono alto, snodato e ho una lingua biforcuta, rappresento la morte e la rinascita.", 
        "Sono una città sul Nilo, famosa per le grandi costruzioni e le piramidi.",
        "Sono il mezzo di trasporto tipico del deserto, utilizzato dagli antichi egizi.",
        "Sono uno degli dei principali, il mio volto è raffigurato con il corpo di falco.",
        "Sono una pietra preziosa molto apprezzata, spesso associata alla regalità e alla protezione.",
        "Sono un corpo celeste notturno, spesso associato a divinità.",
        "Sono un monumento funerario complesso, costruito per onorare i defunti.",
        "Qual è uno strumento musicale più antico, utilizzato spesso nelle cerimonie religiose?",
        "Qual è il vento caratteristico del Sahara, considerato un vento divino, che annuncia la venuta della stagione delle inondazioni?",
        "L'animale sacro, associato alla dea della maternità e della famiglia?",
        "Sono la divinità della morte, guido le anime nel regno dei morti.",
        "Qual è simbolo del ciclo eterno della vita, la cui forma rappresenta l'infinito?", 
        "Gioco di strategia associato ai faraoni?", 
        "Lo strumento agricolo, utilizzato per irrigare i campi lungo il Nilo?",
        "Come si chiama il tipo di imbarcazione utilizzata dagli antichi egizi per il commercio e il trasporto?",
        "Sono una città costiera, famosa per il mio tempio dedicato a una grande dea."
    };

    const char *soluzioni[] = {
        "4",  // Quante punte ha la piramide?
        "Ra",  // Chi era il dio egizio del sole?
        "Gatto",  // Quale era l'animale sacro per gli antichi egizi?
        "Nilo",  // Qual è il nome del fiume che scorre attraverso l'antico Egitto?
        "4",  // Quanti dei egizi sono presenti alla creazione?
        "Geroglifici",  // Qual è l'antico sistema di scrittura egizio?
        "Sepoltura",  // Qual era la funzione principale delle piramidi?
        "Tebe",  // Qual è il nome della città sacra degli antichi egizi?
        "Isis",  // Qual era la principale divinità femminile egizia?
        "Spirito",  // Cosa significa la parola 'Ka'?
        "Potere",  // Cosa rappresenta l'obelisco nell'antico Egitto?
        "Khonshu",  // Chi era considerato il dio della luna?
        "Thot",  // Chi era la dea della magia?
        "Faraoni", // Sono gli antichi sovrani dell'Antico Egitto, spesso sepolti in imponenti tombe chiamate valle dei re. Chi sono?
        "Sfinge", // Quali sono i giganteschi monumenti con teste umane e corpi di leone, poste all'ingresso di templi e tombe?
        "Scarabei", // Quali insetti sono il simbolo della resurrezione e della rinascita nell'Antico Egitto?
        "Ba", // Sono gli spiriti benevoli che assistono i defunti nel loro viaggio nell'aldilà. Chi sono?
        "Talismano", // Cosa è un piccolo idolo di argilla o legno usato come amuleto per garantire fortuna e protezione?
        "Scettri", // I bastoni ricurvi utilizzati dagli antichi egizi come simbolo di autorità e potere.
        "Libro dei Morti", // Complesso testo sacro e rituale dell'Antico Egitto, utilizzato nei riti religiosi e funerari.
        "Fiori di Loto", // Sono i fiori simbolici dell'Antico Egitto, spesso associati a rinascita e fertilità.
        "Sole", // Nasce al mattino, muore alla sera, è il simbolo della vita e dell'eternità.
        "Ibis", // Molti cercano il mio segreto, sono un simbolo di saggezza e misticismo, la mia testa è adornata di piume.
        "Clessidra", // Misuro il tempo con la sabbia, segno il passaggio delle ere.,
        "Coccodrilo", // Mi trovi nel Nilo, ho una pelle dura e proteggo i tesori nascosti.
        "Iside", // Indosso un copricapo bianco e ho un corno d'abbondanza, simbolo di fertilità e prosperità.
        "Serpente", // Sono alto, snodato e ho una lingua biforcuta, rappresento la morte e la rinascita.
        "Giza", // Sono una città sul Nilo, famosa per le grandi costruzioni e le piramidi.
        "Dromedario", // Sono il mezzo di trasporto tipico del deserto, utilizzato dagli antichi egizi.
        "Horus", // Sono uno degli dei principali, il mio volto è raffigurato con il corpo di falco.
        "Lapislazzuli", // Sono una pietra preziosa molto apprezzata, spesso associata alla regalità e alla protezione.
        "Luna", // Sono un corpo celeste notturno, spesso associato a divinità.
        "Mastaba", // Sono un monumento funerario complesso, costruito per onorare i defunti.
        "Arpa", // Qual è uno strumento musicale più antico, utilizzato spesso nelle cerimonie religiose?
        "Khamsin", // Qual è il vento caratteristico del Sahara, considerato un vento divino, che annuncia la venuta della stagione delle inondazioni?
        "Ippopotamo", // L'animale sacro, associato alla dea della maternità e della famiglia?
        "Anubi", // Sono la divinità della morte, guido le anime nel regno dei morti.
        "Ankh", // Qual è simbolo del ciclo eterno della vita, la cui forma rappresenta l'infinito?
        "Senet", // Gioco di strategia associato ai faraoni?
        "Shaduf", // Lo strumento agricolo, utilizzato per irrigare i campi lungo il Nilo?
        "Feluca", // Come si chiama il tipo di imbarcazione utilizzata dagli antichi egizi per il commercio e il trasporto?
        "Alejandria" // Sono una città costiera, famosa per il mio tempio dedicato a una grande dea.
    };

    const char *enigmi_risposta_multipla[] = {
        "Quale regina è famosa per aver costruito molti monumenti?",
        "Qual è il significato del simbolo della croce ansata?",
        "Cosa significa 'Ankh'?",
        "Chi fu l'ultima regina dell'antico Egitto?",
        "Chi era il dio egizio della morte?",
        "Qual era il titolo della regina egizia associata a una famosa maschera funeraria?",
        "Cosa simboleggiava il dio egizio Horus?",
        "Qual era il titolo della regina associata al faraone Tutankhamon?",
        "Cosa rappresentava la divinità egizia Bastet?",
        "Chi era considerato il dio della fertilità nell'antico Egitto?"
    };

    const char *soluzione_risposta_multipla[] = {
        "Hatshepsut",  // Quale regina è famosa per aver costruito molti monumenti?
        "Vita, morte, rinascita",  // Qual è il significato del simbolo della croce ansata?
        "Vita",  // Cosa significa 'Ankh'?
        "Cleopatra",  // Chi fu l'ultima regina dell'antico Egitto?
        "Osiride",  // Chi era il dio egizio della morte?
        "Nefertiti",  // Qual era il titolo della regina egizia associata a una famosa maschera funeraria?
        "Protezione e guarigione",  // Cosa simboleggiava il dio egizio Horus?
        "Regina Ankhesenamun",  // Qual era il titolo della regina associata al faraone Tutankhamon?
        "Gatto",  // Cosa rappresentava la divinità egizia Bastet?
        "Min"  // Chi era considerato il dio della fertilità nell'antico Egitto?
    };

    const char* errata1[] = {
        "Nefertiti", // Quale regina è famosa per aver costruito molti monumenti?
        "Resurrezione",  // Qual è il significato del simbolo della croce ansata?
        "Morte",  // Cosa significa 'Ankh'?
        "Hatshepsut", // Chi fu l'ultima regina dell'antico Egitto?
        "Ra",  // Chi era il dio egizio della morte?
        "Cleopatra", // Qual era il titolo della regina egizia associata a una famosa maschera funeraria?
        "Forza e potenza",  // Cosa simboleggiava il dio egizio Horus?
        "Regina Nefertari", // Qual era il titolo della regina associata al faraone Tutankhamon?
        "Cane",  // Cosa rappresentava la divinità egizia Bastet?
        "Hathor"  // Chi era considerato il dio della fertilità nell'antico Egitto?
    };

    const char* errata2[] = {
        "Cleopatra", // Quale regina è famosa per aver costruito molti monumenti?
        "Morte",  // Qual è il significato del simbolo della croce ansata?
        "Miracolo",  // Cosa significa 'Ankh'?
        "Nefertari", // Chi fu l'ultima regina dell'antico Egitto?
        "Anubi",  // Chi era il dio egizio della morte?
        "Nefertiti", // Qual era il titolo della regina egizia associata a una famosa maschera funeraria?
        "Saggezza e conoscenza",  // Cosa simboleggiava il dio egizio Horus?
        "Regina Cleopatra", // Qual era il titolo della regina associata al faraone Tutankhamon?
        "Falco",  // Cosa rappresentava la divinità egizia Bastet?
        "Thoth"  // Chi era considerato il dio della fertilità nell'antico Egitto?
    };

    const char* errata3[] = {
        "Nefertari", // Quale regina è famosa per aver costruito molti monumenti?
        "Cambiamento, scorrere e tempo", // Qual è il significato del simbolo della croce ansata?
        "Grazia",  // Cosa significa 'Ankh'?
        "Thoth", // Chi fu l'ultima regina dell'antico Egitto?
        "Horus",  // Chi era il dio egizio della morte?
        "Cleopatra", // Qual era il titolo della regina egizia associata a una famosa maschera funeraria?
        "Guerra e vittoria",  // Cosa simboleggiava il dio egizio Horus?
        "Regina Nefertiti", // Qual era il titolo della regina associata al faraone Tutankhamon?
        "Ibis",  // Cosa rappresentava la divinità egizia Bastet?
        "Ra"  // Chi era considerato il dio della fertilità nell'antico Egitto?
    };

    // Il tipo di enigma in questo caso non può essere 2, ma non può essere nemmeno minore di 0

    if(type < 0)
        type = 0;

    if(type > 1)
        type = 1;
    
    enigma->type = type;

    if(type == 0){
        enigma->attempts_left = 1;

        indice = my_random(0, numero0 - 1); // Sceglie l'indovinello

        enigma->question = (char*) malloc((strlen(enigmi[indice]) + 1) * sizeof(char));
        if(enigma->question == NULL)
            return -1;
        strcpy(enigma->question, enigmi[indice]);

        enigma->solution = (char*) malloc((strlen(soluzioni[indice]) + 1) * sizeof(char));
        if(enigma->question == NULL){
            free(enigma->question);
            return -1;
        }
        strcpy(enigma->solution, soluzioni[indice]);

        for(; i < 4; i++)
            enigma->options[i] = NULL;
    }
    else if(type == 1){
        enigma->attempts_left = 3; // 3 tentativi

        indice = my_random(0, numero1 - 1); // Sceglie l'indovinello
        
        enigma->question = (char*) malloc((strlen(enigmi_risposta_multipla[indice]) + 1) * sizeof(char));
        if(enigma->question == NULL)
            return -1;
        strcpy(enigma->question, enigmi_risposta_multipla[indice]);
    
        enigma->solution = (char*) malloc((strlen(soluzione_risposta_multipla[indice]) + 1) * sizeof(char));
        if(enigma->question == NULL){
            free(enigma->question);
            return -1;
        }
        strcpy(enigma->solution, soluzione_risposta_multipla[indice]);

        // Crea le opzioni
        enigma->options[0] = (char*) malloc((strlen(soluzione_risposta_multipla[indice]) + 1) * sizeof(char));
        if(enigma->options[0] == NULL){
            free(enigma->question);
            free(enigma->solution);
            return -1;
        }
        strcpy(enigma->options[0], soluzione_risposta_multipla[indice]);

        enigma->options[1] = (char*) malloc((strlen(errata1[indice]) + 1) * sizeof(char));
        if(enigma->options[1] == NULL){
            free(enigma->question);
            free(enigma->solution);
            free(enigma->options[0]);
            return -1;
        }
        strcpy(enigma->options[1], errata1[indice]);

        enigma->options[2] = (char*) malloc((strlen(errata2[indice]) + 1) * sizeof(char));
        if(enigma->options[2] == NULL){
            free(enigma->question);
            free(enigma->solution);
            free(enigma->options[0]);
            free(enigma->options[1]);
            return -1;
        }
        strcpy(enigma->options[2], errata2[indice]);

        enigma->options[3] = (char*) malloc((strlen(errata3[indice]) + 1) * sizeof(char));
        if(enigma->options[3] == NULL){
            free(enigma->question);
            free(enigma->solution);
            free(enigma->options[0]);
            free(enigma->options[1]);
            free(enigma->options[2]);
            return -1;
        }
        strcpy(enigma->options[3], errata3[indice]);
        
        // Mescola le opzioni
        for(; i < 3; i++){
            j = my_random(3, i);
            swap_string(&(enigma->options[i]), &(enigma->options[j]));
        }
    }

    return 0;
}

// Crea la tomba all'interno dell'ultima stanza
int create_ancient_egypt_tomb(struct object* item){
    int i = 0;
    const char* tomba = "Sepolcro_di_Faraone";
    const char* descrizione_tomba = "La tomba della faraone è una sontuosa camera funeraria, situata al centro di una piramide imponente e intricata.";
    const char* indovinello = "Chi cammina prima su quattro zampe, poi su due e infine su tre?";
    const char* soluzione = "Uomo";

    if(item == NULL)
        return -1;

    // Generazione dell'oggetto
    item->type = 3; // Per indicare che è speciale, poiché è l'oggetto per uscire

    item->name = (char*) malloc((strlen(tomba)) * sizeof(char)); 
    if(item->name == NULL)
        return -1;
    strcpy(item->name, tomba);

    item->description = (char*) malloc((strlen(descrizione_tomba)) * sizeof(char)); 
    if(item->description == NULL){
        free(item->name);
        return -1;
    }
    strcpy(item->description, descrizione_tomba);
          
    item->usable = 0;
    item->player = -1;
    item->bonus_time = 0;

    item->opens_door = 0;
    item->location_index = -1;

    // Generazione dell'indovinello
    item->riddle_answer = 1;
    item->enigma.type = 0;
    item->enigma.question = (char*) malloc((strlen(indovinello) + 1)  * sizeof(char));
    if(item->enigma.question == NULL){
        free(item->name);
        free(item->description);
        return -1;
    }
    strcpy(item->enigma.question, indovinello);

    item->enigma.solution = (char*) malloc((strlen(soluzione) + 1) * sizeof(char));
    if(item->enigma.solution == NULL){
        free(item->name);
        free(item->description);
        free(item->enigma.question);
        return -1;
    }
    strcpy(item->enigma.solution, soluzione);
    
    for(; i < 4; i++)
        item->enigma.options[i] = NULL;
    
    item->enigma.attempts_left = 1;

    return 0;
}

// Crea la descrione della stanza
int create_ancient_egypt_description(struct location* l){
    int dimensione = 0, i = 0;
    const char* presentazione = "Ti trovi nella ++";
    const char* continuo = "++. \nGuardandoti intorno vedi che c'è molta polvere. Sicuramente è un luogo molto antico. La luce che penetra dalle poche fessure riesce a farti vedere a malapena qualcosa. All'interno puoi trovare: \n";
    const char* asterischi = "**";
    const char* asterischi_continuo = "**: ";
    const char* a_capo = "\n";
    const char* finale = "Ci sono delle aperture per andare nelle altre stanze. Usa il comando doors per vedere dove puoi andare.\n";

    dimensione += strlen(presentazione) + strlen(l->name) + strlen(continuo) + strlen(finale); // Prendo la dimensione dalle stringhe precedentemente definite

    for(; i < l->num_items; i++) // Aggiunge la lunghezza dei nomi e delle descrizioni degli oggetti nella location
        dimensione += strlen(l->items[i].name) + strlen(l->items[i].description) + 7; // **nome oggetto**: descrizione\n
    

    l->description = (char*) malloc((dimensione + 1) * sizeof(char));
    if(l->description == NULL)
        return -1;

    // Crea la descrizione in sé per sé
    strcpy(l->description, presentazione);
    strcat(l->description, l->name);
    strcat(l->description, continuo);
    for(i = 0; i < l->num_items; i++){
        strcat(l->description, asterischi);
        strcat(l->description, l->items[i].name);
        strcat(l->description, asterischi_continuo);
        strcat(l->description, l->items[i].description);
        strcat(l->description, a_capo);
    }
    strcat(l->description, finale);
    l->description[dimensione] = '\0';
    
    return 0;
}

// Inizializza il descrittore di una Escape Room a seconda dell'indice inserito
int initialize_escape_room(struct escape_room_descriptor* r, const int index){
    int ret = -1;
    if(index < 0 || index > N_ROOMS)
        return -1;
    if(index == 0)
        ret = initialize_medieval_escape_room(r);
    if(index == 1)
        ret = initialize_ancient_egypt_escape_room(r);        

    return ret;
}

// Crea un Escape Room a seconda dell'indice inserito
int create_escape_room(struct escape_room_descriptor* r, const int index){
    int ret = -1;
    if(index < 0 || index > N_ROOMS)
        return -1;
    if(index == 0)
        ret = create_medieval_escape_room(r);
    if(index == 1)
        ret = create_ancient_egypt_escape_room(r);        

    return ret;
}

// Distrugge l'escape room
void destroy_escape_room(struct escape_room_descriptor* r){
    int i = 0, j = 0;
    if(r == NULL)
        return;

    for(i = 0; i < r->num_rooms; i++){
        // Dealloca le stringhe presenti nelle stanze (nome e descrizione)
        if(r->rooms[i].name != NULL)
            free(r->rooms[i].name);

        if(r->rooms[i].description != NULL)
            free(r->rooms[i].description);

        // Dealloca gli oggetti presenti nell'escape room
        for(j = 0; j < r->rooms[i].num_items; j++)
            destroy_object(&r->rooms[i].items[j]);

        // Dealloca le locazioni collegate alle porte
        if(r->rooms[i].linked_locations != NULL)
            free(r->rooms[i].linked_locations);
    }

    // Dealloca il nome del tema e la descrizione generale dell'escape room
    if(r->theme != NULL)
        free(r->theme);

    if(r->description != NULL)
        free(r->description);

    // Dealloca la memoria dell'array delle stanze e infine dell'escape room stessa
    if(r->rooms != NULL)
        free(r->rooms);
    
    r = NULL;
}

// Distrugge un oggetto
void destroy_object(struct object* o){
    int i = 0;

    // Dealloca le stringhe per gli enigmi presenti negli oggetti, se ci sono
    if(o->enigma.question != NULL)
        free(o->enigma.question);

    if(o->enigma.solution != NULL)
        free(o->enigma.solution);

    // Dealloca le opzioni per le risposte a scelta multipla
    if(o->enigma.type == 1){
        for(; i < 4; i++){
            if(o->enigma.options[i] != NULL)
                free(o->enigma.options[i]);
        }
    }

    // Dealloca il nome degli oggetti e le stringhe di descrizione
    if(o->name != NULL)
        free(o->name);

    if(o->description != NULL)
        free(o->description);
}

// Riscrive la descrizione di una stanza
void rewrite_description(struct location* l, const int index){
    int ret; 
    char* copia; // Tiene la precedente descrizione se presente
    
    // Verifica se la struttura location è valida e se l'indice è nel range consentito
    if(l == NULL || index < 0 || index > N_ROOMS)
        return;

    // Si usa solo per l'escape room egiziana
    if(index == 1){
        // Se la descrizione attuale non è nulla, crea una copia di essa
        if(l->description != NULL){
            copia = (char*) malloc((strlen(l->description) + 1) * sizeof(char));
            if(copia == NULL)
                return;
            strcpy(copia, l->description);
            free(l->description);
        }
        else
            copia = NULL;

        // Chiamata ad una funzione specifica alla Escape Room Egizia per riscrivere la descrizione
        ret = create_ancient_egypt_description(l); 
        if(ret < 0){
            // In caso di errore, ripristina la descrizione precedente o almeno ci prova
            if(copia == NULL)
                l->description = NULL;
            else{
                if(l->description != NULL)
                    free(l->description);
                l->description = (char*) malloc((strlen(copia) + 1) * sizeof(char));
                if(l->description == NULL){
                    free(copia);
                    return;
                }
                strcpy(l->description, copia);
            }
        }
        
        // Libera la memoria della copia
        if(copia != NULL)
            free(copia);
    }
}


// Le Escape Room sono espandibili. Attualmente ne sono presenti 2, tuttavia se ne possono creare altre. Innanzitutto bisogna aumentare la costante globale N_ROOMS. 
// Poi bisogna modificare le funzioni "create_escape_room" e "initialize_escape_room" affinché creino la nuova Escape Room. Per creare la nuova Escape Room c'è 
// bisogno di tenere a mente che nell'ultima stanza bisogna mettere sempre un oggetto di tipo 3 (solo uno di tipo 3), che servirà a poter uscire. Inoltre i 
// tentativi rappresentati da "riddle.attempts_left" sono validi solo se il tipo dell'oggetto è 1. Se si blocca una location mettendo "door.is_blocked" a 1, bisogna
// ricordarsi  di inserire qualcosa che apra la location, mettendo in un object "object.open_doors" a 1 e index_location con l'indice della location a cui porta 
// la door che è bloccata, nella variabile "door.next_location".
// Se un oggetto da tempo_bonus, non deve dare punti e viceversa, tuttavia questa cosa può cambiare se si modifica adeguatamente il comando "take". 
// "object.player" và sempre inizializzato a -1 o comunque ad un numero minore di 0.
// Ricordarsi che "location.is_final" è sempre da inizializzare a 1 se la stanza è quella finale (e deve avere un oggetto di tipo 3) o a 0 se non lo è.
// Cercate per quanto possibile di mettere nomi diversi alle varie location e agli oggetti. 
// Gli oggetti che hanno un nome composto da più parole devono essere divisi tramite underscore (_).
// Se una location possiede più di un oggetto, ricordati di implementare una funzione per cambiare la descrizione della stanza nel caso vi siano indicati gli
// oggetti. Guarda rewrite_description. 
// Ovviamente è consigliabile un buon playtesting della nuova Escape Room. 

// INIZIO PROGRAMMA

int main(int argc, char* argv[]){
    int port, listener;

    // Verifica se il numero corretto di argomenti è stato fornito
    if(argc != 2){
        printf("Errore! Per attivare l'applicazione c'è bisogno che tu inserisca: \n");
        printf("./%s <port> \n", argv[0]);
        printf("<port> deve essere un numero valido per una porta! \n");
        exit(EXIT_FAILURE);
    }

    // Estrai il numero di porta dalla riga di comando
    port = atoi(argv[1]);

    // Inizializza il buffer e stampa un messaggio di apertura del Server
    memset(buffer, 0, sizeof(buffer));
    printf("Apertura del Server dell'applicazione ESCAPE ROOM! \n");  
    // Crea il database
    create_database();
    // Inizializza il generatore di numeri
    srand(time(NULL));

    // Avvia il server principale sulla porta specificata
    listener = start_main_server(port);
    // Chiude il Main Server o stampa un errore
    if(listener != -1)
        close_main_server(listener);
    else
        printf("Errore nell'inserimento della porta! \n");
    return 0;
}


// Main Server

// Funzione per avviare il Main Server
int start_main_server(const int port){
    int listener, newfd, ret, codice = 0, i;   
    struct sockaddr_in server_address, client_address; 
    socklen_t addrlen = sizeof(struct sockaddr_in);

    struct users *lista_utenti = NULL;

    fd_set master; // Set di descrittori da monitorare    
    fd_set read_fds; // Set dei descrittori pronti    
    int fdmax; // Descrittore massimo

    // Verifica se la porta è valida
    if(port < 0)
        return -1;

    // Chiamata alla funzione per creare e collegare il socket
    listener = create_and_bind_socket(port, &server_address, 1);
    if(listener < 0)
        exit(EXIT_FAILURE);

    // Inizializza la struttura per il Main Server e i Game Server
    initialize_main_server_descriptor(servers, listener, port);
    initialize_game_servers_descriptor(servers);
    printf("Avvio Main Server... \n");

    // Mette in ascolto il socket e crea una coda di attesa di MAX_CLIENTS * (MAX_SERVERS - 1) richieste
    ret = listen(listener, MAX_CLIENTS * (MAX_SERVERS - 1));
    if(ret < 0){
        perror("Main Server: Errore nell'ascolto: \n");
        exit(EXIT_FAILURE);
    }    

    // Stampa un messaggio di avvio e le istruzioni
    printf("***************************** SERVER STARTED ********************************* \n");
    print_commands();
    printf("****************************************************************************** \n");

    // Reset dei descrittori
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    
    // Aggiunge il socket di ascolto 'listener' e l'input da tastiera ai descrittori da monitorare
    FD_SET(listener, &master); 
    FD_SET(STDIN_FILENO, &master); 
    
    // Traccia il nuovo fdmax
    fdmax = listener > STDIN_FILENO ? listener : STDIN_FILENO;

    while(1){
        // Imposta il set di socket da monitorare in lettura per la select()
        read_fds = master;   

        // Si blocca in attesa di descrittori pronti. Attesa ***senza timeout*** (ultimo parametro attuale 'NULL')
        ret = select(fdmax + 1, &read_fds, NULL, NULL, NULL); 
        if(ret < 0){ 
            perror("Main Server: ERRORE SELECT"); 
            exit(EXIT_FAILURE);
        }

        // Scansiona ogni descrittore 'i'
        for(i = 0; i <= fdmax; i++){ 
            if(FD_ISSET(i, &read_fds)){
                // Se c'è attività sulla tastiera
                if(FD_ISSET(STDIN_FILENO, &read_fds)){
                    ret = handle_server_commands(servers, &master, fdmax);
                    if(ret < 0)
                        break;
                }
                // Se c'è attività sul socket di ascolto, accetta una nuova connessione
                else if(FD_ISSET(listener, &read_fds)){
                    fflush(stdout);
                    printf("Main Server: Client rilevato! \n");
                    addrlen = sizeof(client_address);
                    newfd = accept(listener, (struct sockaddr *)&client_address, &addrlen);
                    if(newfd < 0){
                        perror("Main Server: Errore nell'accettazione di una richiesta");
                        continue;
                    }
                    FD_SET(newfd, &master);
                    servers[0].clients++;

                    if(newfd > fdmax)
                        fdmax = newfd;                        
                    
                    ret = 0;
                }
                // Collegamento da parte del Client per essere collegato ad un Game Server
                else{
                    fflush(stdout);
                    
                    // Il codice indica cosa il Main Server deve fare
                    ret = recv_all(i, &codice, sizeof(codice), 0);
                    if(ret <= 0)
                        codice = -1;

                    // Caso di errore
                    if(codice < 0){
                        close(i);
                        FD_CLR(i, &master);
                        servers[0].clients--;
                        ret = 0;
                        accidental_logout(i, &lista_utenti);
                        continue;
                    }
                    // Caso di signup/login/exit
                    else if(codice == 0){
                        ret = handle_client(i, &lista_utenti);    
                        if(ret < 0){
                            close(i);
                            FD_CLR(i, &master);
                            servers[0].clients--;
                            ret = 0;
                            continue;
                        }
                    }                       
                    // Caso in cui il Client cerca di connettersi ad una Escape Room
                    else if(codice == 1){
                        ret = send_rooms_informations(servers, i);
                        if(ret < 0){
                            close(i);
                            FD_CLR(i, &master);
                            servers[0].clients--;
                            ret = 0;
                            accidental_logout(i, &lista_utenti);
                            continue;
                        }
                    }     
                    // Caso in cui si è deciso e viene collegato al Game Server relativo a quella Escape Room
                    else if(codice == 2){
                        ret = connection_to_game_server(i);
                        if(ret < 0){
                            close(i);
                            FD_CLR(i, &master);
                            servers[0].clients--;
                            ret = 0;
                            accidental_logout(i, &lista_utenti);
                            continue;
                        }
                    }
                    // Caso in cui chiede il logout
                    else if(codice == 3){
                        logout(i, &lista_utenti);
                        close(i);
                        FD_CLR(i, &master);
                        servers[0].clients--;
                    }
                        
                    ret = 0;
                }
            }
        } 

        // Controlla i server di gioco
        control_game_servers(servers);

        if(ret < 0)
            break;            
    }

    // Libera la memoria allocata per gli utenti
    free_users(&lista_utenti);

    // Chiude tutti i descrittori
    for(i = 0; i <= fdmax; i++){
        if(FD_ISSET(i, &master) && i != listener)
            close(i);
    }

    // Reset dei descrittori
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // Restituisce il descrittore del listener
    fflush(stdout);
    return listener;
}


void print_commands(){
    printf("Digita un comando: \n");
    printf("1) start <port> --> avvia il server di gioco \n");
    printf("2) stop --> termina il server \n");    
}

// Gestisce i comandi presi da tastiera per il Server
int handle_server_commands(struct server_descriptor* s, fd_set* master, const int fdmax){
    char input[12];
    int port = -1, ret, i, stanza, c;
    char *token; // Puntatore per il token estratto da strtok
    pid_t pid; // Identificatore del processo
 
    fgets(input, sizeof(input), stdin);    
    input[strcspn(input, "\n")] = '\0'; // Rimuovi il carattere di nuova riga dalla fine dell'input

    // Estrae il primo token dall'input
    token = strtok(input, " ");
    if(token != NULL){
        // Se il comando è "start"
        if(strcmp(token, "start") == 0){
            token = strtok(NULL, " "); // Estrae il token successivo che è la porta

            if(token != NULL){
                port = atoi(token); // Converte il token in un intero e lo rende la porta
                // Se la porta è minore rispetto a MIN_ACCESSIBLE_PORT la porta non è valida e quindi si esce
                if(port < MIN_ACCESSIBLE_PORT){
                    printf("Porta non valida \n");
                    return 1; // Non ritorna 0 perché non è andata a buon fine, ma nemmeno -1 perché tutto sommatto non è un errore invalidante
                }      
                
                // Mostra le escape rooms disponibili
                printf("Quale room vuoi implementare nel Game Server? \n\n");
                if(expose_escape_rooms() == -1){
                    printf("Fallimento nella creazione dell'Escape Room \n");
                    return -1;
                }
                
                // Legge il valore della stanza
                printf("Inserisci il valore della room:  ");
                while(scanf("%d", &stanza) != 1 || stanza < 0 || stanza >= N_ROOMS){
                    printf("Valore della room non valido. Riprova!\n");

                    // Pulisci il buffer da input
                    while((c = getchar()) != '\n' && c != EOF);
                    printf("Inserisci il valore della room:  ");
                }

                // Creiamo un processo figlio che gestisca il Game Server
                pid = fork();
                
                // Il processo padre esce, mentre il processo figlio si occupa del nuovo Game Server

                if(pid == -1){
                    // In caso di errore
                    perror("Creazione del processo figlio fallita \n");
                    return -1;
                }
                else if(pid == 0){
                    // Processo figlio
                    srand(time(NULL));

                    // Chiude i descrittori nei master set
                    for(i = 0; i <= fdmax; i++){
                        if(FD_ISSET(i, master))
                            close(i);
                    }
                    FD_ZERO(master);

                    // Avvia il Game Server e gestisce l'Escape Room
                    ret = start_game_server(servers, port, stanza);
                    if(ret < 0)
                        exit(EXIT_FAILURE);
                    else if(ret == 0)
                        exit(EXIT_FAILURE);
                    
                    game_escape_room(servers, ret, stanza);
                }
                else{
                    // Processo padre
                    if(search_room(s, port) < 0 && is_process_running(pid) == 1){ 
                        ret = search_room(s, 0);
                        game_servers_running++;
                        s[ret].port = port; 
                        s[ret].pid = pid; 
                        initialize_escape_room(&s[ret].escape_room, stanza);
                    }
                }                               
            } 
            else{
                printf("Comando non valido! \n");
                print_commands();
                printf("Nota che <port> deve essere il numero della porta desiderata. \n");
                return 1; // Non ritorna 0 perché non è andata a buon fine, ma nemmeno -1 perché tutto sommatto non è un errore invalidante
            }
                
        }
        // Se il comando è "stop"
        else if(strcmp(token, "stop") == 0){
            if(game_servers_running == 0)
                return -1; // Restituisce -1 se non ci sono server attivi
            else{
                // Questo ciclo va avanti finché ci sono Game Server attivi
                while(game_servers_running > 0){
                    // Questo ciclo 'spenge' i Game Server. Se un Game Server non si può spengere perché in partita, prosegue e va avanti. 
                    for(i = 1; i < MAX_SERVERS; i++){
                        // Controlla che ci sia un processo che gestisce un Game Server e nel caso controlla se si è interrotto
                        if(s[i].pid > -1 && is_process_running(s[i].pid) == 1){
                            // Se non si è interrotto entra qua
                            printf("Main Server: Mando il segnale di arresto al Game Server %d \n", i);
                            // Manda un segnale al processo per distruggerlo
                            wake_up_process(s[i].pid);
                            // Dà del tempo al Game Server per stopparsi. 
                            // Questo rallentamento è fatto per dare tempo al processo del Game Server di gestire il segnale di arresto e terminare in modo corretto. 
                            // Dopo la pausa, verifica nuovamente se il processo del Game Server è ancora in esecuzione.
                            sleep(1); 
                            // Se non è più in esecuzione diminuisce "game_servers_running" e mette il pid a -1
                            if(is_process_running(s[i].pid) != 1){
                                game_servers_running--;
                                s[i].pid = -1;
                            }                            
                        }
                    }

                    // Se ci sono Game Server ancora attivi aspetta 5 secondi prima di far ripartire il ciclo, per non stressare di richieste il Game Server
                    if(game_servers_running != 0)
                        sleep(5);
                }

                return -1; // Serve per uscire al Main Server e permettergli di chiudersi in sicurezza
            }
        }
        else{
            printf("Comando non valido! \n");
            print_commands();
            printf("Nota che <port> deve essere il numero della porta desiderata. \n");
            return 1; // Non ritorna 0 perché non è andata a buon fine, ma nemmeno -1 perché tutto sommatto non è un errore invalidante
        }
    }

    return 0;
}

// Gestisce i messaggi inviati dal Client
int handle_client(const int newfd, struct users** utenti){
    int ret = -1;
    const char *msg_signup = "Registrazione";
    const char *msg_login = "Login";
    const char *msg_exit = "Il Client si è disconnesso";
    const char *msg_error = "ERRORE DEL CLIENT!";

    // Ricevo un messaggio dal client e lo nel buffer, poiché indicherà il protocollo da seguire
    ret = recv_msg(newfd, buffer, 0);
    if(ret < 0)
        return ret;

    // Verifica il contenuto del buffer per determinare l'azione da eseguire
    if(strncmp(buffer, "signup", 6) == 0 || strncmp(buffer, "Signup", 6) == 0 || strncmp(buffer, "SIGNUP", 6) == 0){
        // Se il messaggio indica la registrazione, esegui la registrazione
        printf("Main Server: %s \n", msg_signup);
        ret = signup(newfd, utenti);
        if(ret != 0)
            printf("Main Server: Registrazione non andata a buon fine \n");
    }
    else if(strncmp(buffer, "login", 5) == 0 || strncmp(buffer, "Login", 5) == 0 || strncmp(buffer, "LOGIN", 5) == 0){
        // Se il messaggio indica il login, esegui il login
        printf("Main Server: %s \n", msg_login);
        ret = login(newfd, utenti);
        if(ret != 0)
            printf("Main Server: Login non andato a buon fine \n");
    }
    else if(strncmp(buffer, "exit", 4) == 0 || strncmp(buffer, "Exit", 4) == 0 || strncmp(buffer, "EXIT", 4) == 0){
        // Se il messaggio indica l'uscita, restituisci -1 per indicare la disconnessione del client
        printf("Main Server: %s \n", msg_exit);
        return -1;
    }
    else{
        // Se il messaggio non corrisponde a nessuna azione nota, restituisci -1
        printf("Main Server: %s \n", msg_error);
        return -1; 
    }

    return 0;
}

// Parte la connessione al Game Server
int connection_to_game_server(const int sockfd){
    int ret, lunghezza = 0, porta, codice = -1, indice = -1;
    char* username;

    // Riceve l'username
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;
    
    // Alloca dinamicamente la memoria per l'username e verifica se l'allocazione ha successo
    lunghezza = strlen(buffer);
    username = (char*) malloc((lunghezza + 1) * sizeof(char));
    if(username == NULL){
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        printf("Main Server: Errore nell'allocazione dello username \n");
        return -1;
    }    
    strcpy(username, buffer);

    codice = 0;
    // Invia una conferma al client indicando che l'username è stato ricevuto e allocato
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        free(username);
        printf("Main Server: Errore nell'allocazione dello username \n");
        return ret;
    }

    // Riceve la porta
    ret = recv_all(sockfd, &porta, sizeof(porta), 0);
    if(ret < 0){
        free(username);
        return -1;
    }

    // Controlla che la porta sia già stata inserita e che sia valida
    indice = search_room(servers, porta); 
    if(indice > 0 && porta >= MIN_ACCESSIBLE_PORT){
        // Invia una conferma al client indicando che la connessione al Game Server è possibile
        codice = 0;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Main Server: Errore nell'invio dell'OK alla connessione");
            free(username);
            return ret;
        }       

        servers[indice].clients++;
        printf("Main Server: Connessione al Game Server %d da parte di %s \n", indice, username);
    }
    else{
        // Se la porta non è valida o non è stata inserita, invia un codice di errore al client
        codice = -1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Main Server: Errore nella ricezione della porta");
            free(username);
            return ret;
        }               
    }

    // Libera la memoria allocata per l'username
    free(username);
    return 0;
}


void close_main_server(const int listener){
    printf("Chiudo il Main Server \n");
    close(listener);
}

// Funzione che si occupa della registrazione
int signup(const int sockfd, struct users** utenti){
    FILE *file;
    int ret = 0, risultato;
    char *username;
    char *password;

    fflush(stdout);

    // Riceve l'username dal Client
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;
    
    // Alloca dinamicamente la memoria per l'username e verifica se l'allocazione ha successo
    username = (char*) malloc((strlen(buffer) + 1) * sizeof(char)); // Alloca la memoria per l'username
    if(username == NULL){
        printf("Errore nell'allocazione della memoria per l'username.\n");
        return -1; 
    }
    printf("Client: %s\n", buffer);
    strcpy(username, buffer);

    // Riceve la password dal Client
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0){
        free(username);
        return ret;
    }
    
    // Alloca dinamicamente la memoria per la password e verifica se l'allocazione ha successo
    password = (char*) malloc((strlen(buffer) + 1) * sizeof(char)); // Alloca la memoria per la password
    if(password == NULL){
        printf("Errore nell'allocazione della memoria per la password.\n");
        free(username);
        return -1; 
    }
    strcpy(password, buffer);

    // Controlla se l'username è già presente nel database
    risultato = search_username(username, utenti, sockfd);
    if(risultato < 0){
        // Invia un codice di errore al client in caso di errore nella ricerca
        ret = send_all(sockfd, &risultato, sizeof(risultato), 0);
        if(ret < 0)
            perror("Errore nell'invio del risultato");
        free(username);
        free(password);
        return -1;
    }
    else if(risultato == 0){
        // Invia un codice di errore al client se l'username è già presente
        risultato = 1;
        ret = send_all(sockfd, &risultato, sizeof(risultato), 0);
        if(ret < 0)
            perror("Errore nell'invio del risultato");
        free(username);
        free(password);
        return -1;
    }

    // Apre il file database in modalità append
    file = fopen(path_database, "a");
    
    // Verifica se il file è stato aperto correttamente
    if(file == NULL){
        risultato = -1;
        printf("Errore nell'apertura del file database! \n");  
        // Invia un codice di errore al client in caso di errore nell'apertura del file 
        ret = send_all(sockfd, &risultato, sizeof(risultato), 0);
        if(ret < 0)
            perror("Errore nell'invio del risultato");
        free(username);
        free(password);
        return -1;
    }

    // Scrive l'username e la password nel file database
    fprintf(file, "%s    %s    \n", username, password);

    // Chiude il file
    fclose(file); 
    free(username);
    free(password);

    // Invia un codice di successo al client
    risultato = 0;
    ret = send_all(sockfd, &risultato, sizeof(risultato), 0);
    if(ret < 0){
        perror("Errore nell'invio del risultato");
        return -1;
    }
    
    memset(buffer, 0, sizeof(buffer));
    return 0;
}

// Funzione che si occupa del login
int login(const int sockfd, struct users** utenti){
    int ret = 0, risultato = 0;
    char *username;
    char *password;

    // Ricezione dello username dal Client
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;
    // Alloca dinamicamente la memoria per l'username e verifica se l'allocazione ha successo
    username = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(username == NULL){
        printf("Errore nell'inserimento dello username!\n");
        risultato = -1;
    }
    strcpy(username, buffer);
    memset(buffer, 0, strlen(username) + 1);

    // Ricezione della password dal Client
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;
    // Alloca dinamicamente la memoria per la password e verifica se l'allocazione ha successo
    password = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(password == NULL){
        printf("Errore nell'inserimento della password!\n");
        if(username != NULL)
            free(username);
        risultato = -1;
    }
    strcpy(password, buffer);
    memset(buffer, 0, strlen(password) + 1);

    // Verifica delle condizioni di errore durante l'inserimento di username e password
    if(risultato != -1){
        // Ricerca dell'utente nel database
        risultato = search_user(username, password, utenti, sockfd);
    }

    // Invio del risultato del login al Client
    ret = send_all(sockfd, &risultato, sizeof(risultato), 0);
    if(ret < 0){
        perror("Errore nell'invio del risultato del login");
        return -1;
    }

    if(risultato == 0)
        printf("Client: %s\n", username);

    if(username != NULL)
        free(username);
    if(password != NULL)
        free(password);

    return risultato;
}

// Funzione che si occupa del logout
void logout(const int sockfd, struct users** utenti){
    int ret;
    char* username;

    // Ricezione dello username dal Client
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0){
        printf("Main Server: Errore nel Logout \n");
        accidental_logout(sockfd, utenti);
        printf("Main Server: Errore nel Logout risolto ... \n");
        return;
    }

    // Alloca dinamicamente la memoria per l'username e verifica se l'allocazione ha successo
    username = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(username == NULL){
        printf("Main Server: Errore nel Logout \n");
        accidental_logout(sockfd, utenti);
        printf("Main Server: Errore nel Logout risolto ... \n");
        return;
    }
    strcpy(username, buffer);

    // Rimozione dell'utente dalla lista degli utenti loggati e liberazione della memoria allocata per lo username
    remove_user(utenti, username);
    free(username);
}

// Funzione che avvia il logout accidentale
void accidental_logout(const int sockfd, struct users** utenti) {
    struct users* current = *utenti;

    while(current != NULL){
        // Trovato l'utente con il sockfd specificato
        if(current->sockfd == sockfd){
            // Chiamiamo remove_user per rimuovere l'utente
            remove_user(utenti, current->username);
            return;
        }
        current = current->next;
    }

    // Se si arriva qui, significa che non è stato trovato alcun utente con il sockfd specificato
    printf("Utente con sockfd %d non trovato. \n", sockfd);
}

// Funzione che cerca un nome utente nel file di database con solo username
int search_username(const char* username, struct users** utenti, const int sockfd){
    FILE *file = fopen(path_database, "r");
    char *stringa = (char*) malloc(strlen(username) + 6); // +6 per username, 4 spazi, '\0'
    int risultato = 1;

    // Verifica se il file di database è stato aperto correttamente
    if(file == NULL){
        if(stringa != NULL)
            free(stringa);
        return -1;
    }
    
    // Verifica se l'allocazione di memoria per la stringa è avvenuta correttamente
    if(stringa == NULL){
        if(file != NULL)
            fclose(file);
        return -1; // Errore nell'allocazione della memoria
    }

    // Costruzione della stringa da cercare nel file di database
    strncpy(stringa, username, strlen(username));
    strcat(stringa, "    ");

    // Scarta le prime due righe del file di database
    fgets(buffer, sizeof(buffer), file); // "DATABASE GIOCATORI"
    fgets(buffer, sizeof(buffer), file); // "USERNAME PASSWORD"

    // Ciclo di lettura del file di database
    while(fgets(buffer, sizeof(buffer), file) != NULL){
        /*if(strstr(buffer, stringa) != NULL){
            risultato = 0;
            break;
        }*/
        if(strncmp(buffer, stringa, strlen(stringa)) == 0){
            risultato = 0;
            break;
        }
    }

    // Aggiunge l'utente alla lista se non è già presente
    if(risultato == 1)
        add_user(utenti, username, sockfd);

    // Libera la memoria allocata per la stringa e chiude il file
    free(stringa);
    fclose(file);
    memset(buffer, 0, sizeof(buffer));
    return risultato; 
}

// Funzione che cerca un utente nel file di database con username e password
int search_user(const char* username, const char* password, struct users** utenti, const int sockfd){
    FILE *file = fopen(path_database, "r");
    char *stringa = (char*) malloc(strlen(username) + strlen(password) + 9); // +9 rispettivamente per 8 spazi e '\0'
    int risultato = -1;
    const struct users* attuale = *utenti;

    // Verifica se il numero massimo di utenti è stato raggiunto
    if(count_users(*utenti) >= MAX_CLIENTS * (MAX_SERVERS-1))
        return -1;

    // Verifica se il file di database è stato aperto correttamente
    if(file == NULL){
        if(stringa != NULL)
            free(stringa);
        return -1;
    }
    
    // Verifica se l'allocazione di memoria per la stringa è avvenuta correttamente
    if(stringa == NULL){
        if(file != NULL)
            fclose(file);
        return -1; // Errore nell'allocazione della memoria
    }

    // Costruzione della stringa da cercare nel file di database
    strcpy(stringa, username);
    strcat(stringa, "    ");
    strcat(stringa, password);
    strcat(stringa, "    ");

    // Scarta le prime due righe del file di database
    fgets(buffer, sizeof(buffer), file); // "DATABASE GIOCATORI"
    fgets(buffer, sizeof(buffer), file); // "USERNAME PASSWORD"

    // Ciclo di lettura del file di database
    while(fgets(buffer, sizeof(buffer), file) != NULL){
        if(strstr(buffer, stringa) != NULL){
            risultato = 0;

            // Verifica se l'utente è già presente nella lista degli utenti loggati
            while(attuale != NULL){
                if(strcmp(attuale->username, username) == 0){
                    risultato = 1;
                    break;
                }
                attuale = attuale->next;
            }

            // Aggiunge l'utente alla lista se non è già presente
            if(risultato == 0)
                add_user(utenti, username, sockfd);

            break;
        }
    }

    // Libera la memoria allocata per la stringa e chiude il file
    free(stringa);
    fclose(file);
    memset(buffer, 0, sizeof(buffer));
    return risultato;
}

// Manda le informazioni sulle Escape Room presenti nei Game Server al Client
int send_rooms_informations(struct server_descriptor* s, const int sockfd){
    int ret, i = 1, codice;
    char* username;

    // Invia il numero di Game Server attualmente in esecuzione
    ret = send_all(sockfd, &game_servers_running, sizeof(game_servers_running), 0);    
    if(ret < 0){
        perror("Main Server: Errore nell'invio delle informazioni riguardanti le Escape Room");
        return ret;
    }

    // Se non ci sono Game Server attivi, esce dalla funzione
    if(game_servers_running <= 0)
        return 0;

    // Riceve l'username dal client
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;

    // Alloca dinamicamente la memoria per l'username e verifica se l'allocazione ha successo
    username = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(username == NULL){
        codice = -1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);    
        if(ret < 0){
            perror("Main Server: Errore nell'invio delle informazioni riguardanti le Escape Room");
            return ret;
        }
        return -1;
    }
    strcpy(username, buffer);
    memset(buffer, 0, strlen(buffer) + 1);

    // Invia un codice di successo al client
    codice = 0;
    ret = send_all(sockfd, &codice, sizeof(codice), 0);    
    if(ret < 0){
        perror("Main Server: Errore nell'invio delle informazioni riguardanti le Escape Room");
        return ret;
    }

    // Itera attraverso i Game Server e invia le informazioni sulle Escape Room al client
    for(; i < MAX_SERVERS; i++){
        if(s[i].port > 0 && s[i].clients < MAX_CLIENTS){
            // Invia il numero di porta del Game Server al client
            ret = send_all(sockfd, &s[i].port, sizeof(s[i].port), 0);    
            if(ret < 0){
                perror("Main Server: Errore nell'invio delle informazioni riguardanti le Escape Room");
                return ret;
            }

            // Invia il tema della Escape Room al client
            ret = send_msg(sockfd, s[i].escape_room.theme, strlen(s[i].escape_room.theme), 0);
            if(ret < 0){
                perror("Main Server: Errore nell'invio delle informazioni riguardanti le Escape Room");
                return ret;
            }

            // Invia la descrizione della Escape Room al client
            ret = send_msg(sockfd, s[i].escape_room.description, strlen(s[i].escape_room.description), 0);
            if(ret < 0){
                perror("Main Server: Errore nell'invio delle informazioni riguardanti le Escape Room");
                return ret;
            }
        }
    }
    
    return 0;
}

// Espone la Escape Room
int expose_escape_rooms(){
    struct escape_room_descriptor* rooms = (struct escape_room_descriptor*) malloc(N_ROOMS * sizeof(struct escape_room_descriptor));
    int ret, i = 0;

    // Viene creato un vettore provvisorio di Escape Room con N_ROOMS elementi per inizializzare il tema e la descrizione e stamparli a video
    if(rooms == NULL)
        return -1;
    
    for(; i < N_ROOMS; i++){
        // Inizializza provvisoriamente le Escape Room e stampa tema e descrizione a video per far decidere quale Escape Room implementa il Game Server
        ret = initialize_escape_room(&rooms[i], i);
        if(ret < 0)
            return ret;
        printf("Escape Room: %d \n", i);
        printf("%s \n", rooms[i].theme);
        printf("Descrizione: %s \n\n", rooms[i].description);
    }

    // Libera il vettore di Escape Room provvisorio
    for(; i < N_ROOMS; i++){
        free(rooms[i].theme);
        free(rooms[i].description);
    }

    free(rooms);
    return 0;
}

// Funzione che controlla lo stato dei Game Server e li riporta a uno stato consistente
void control_game_servers(struct server_descriptor* s){
    int i = 1;
    
    // Itera attraverso l'array di server
    for(; i < MAX_SERVERS; i++){
        // Se il processo del Game Server è terminato
        if(s[i].pid > 0 && is_process_running(s[i].pid) != 1){
            // Resetta le informazioni relative al Game Server
            s[i].port = 0;
            s[i].clients = 0;
            s[i].sockfd = -1;
            s[i].main = 0;
            s[i].pid = -1;
            
            // Libera la memoria allocata per il tema e la descrizione della Escape Room
            free(s[i].escape_room.theme);
            free(s[i].escape_room.description);
            
            // Decrementa il contatore dei Game Server attivi
            game_servers_running--;
        }
        // Se il processo del Game Server non è mai partito e la porta è diversa da 0
        else if(s[i].pid < 0 && s[i].port != 0){
            // Resetta le informazioni relative al Game Server
            s[i].port = 0;
            s[i].clients = 0;
            s[i].sockfd = -1;
            s[i].main = 0;
            s[i].pid = -1;
        }
    }
}

// Crea e restituisce un nuovo nodo utente con il nome fornito
struct users* create_user(const char *nome_utente, const int sockfd){
    struct users *nuovo_utente = (struct users *) malloc(sizeof(struct users));

    // Verifica se la memoria è stata allocata correttamente
    if(nuovo_utente == NULL){
        printf("Memoria insufficiente.\n");
        return NULL;
    }

    // Duplica la stringa del nome utente e inizializza il nodo
    nuovo_utente->username = strdup(nome_utente);
    nuovo_utente->sockfd = sockfd;
    nuovo_utente->next = NULL;

    return nuovo_utente;
}

// Aggiunge un nuovo utente alla testa della lista
void add_user(struct users **lista, const char *nome_utente, const int sockfd){
    // Crea un nuovo nodo utente
    struct users* nuovo = create_user(nome_utente, sockfd);

    // Verifica se il nodo è stato creato correttamente
    if(nuovo == NULL)
        return;

    // Aggiunge il nuovo nodo alla testa della lista
    nuovo->next = *lista;
    *lista = nuovo;
}

// Rimuove un utente con il nome specificato dalla lista
void remove_user(struct users **lista, const char *nome_utente){
    struct users* attuale = *lista;
    struct users* precedente = NULL;

    while(attuale != NULL){
        // Trova l'utente con il nome specificato
        if(strcmp(attuale->username, nome_utente) == 0){
            // Rimuove il nodo dalla lista
            if(precedente == NULL) 
                *lista = attuale->next;
            else
                precedente->next = attuale->next;

            // Libera la memoria allocata per il nodo e il nome utente
            free(attuale->username);
            free(attuale);
            return;
        }

        // Passa al nodo successivo
        precedente = attuale;
        attuale = attuale->next;
    }

    printf("Utente non trovato.\n");
}

// Conta il numero di utenti nella lista
int count_users(const struct users* lista){
    int conteggio = 0;
    const struct users *attuale = lista;

    // Conta il numero di nodi nella lista
    while(attuale != NULL){
        conteggio++;
        attuale = attuale->next;
    }

    return conteggio;
}

// Libera la memoria allocata per tutti i nodi nella lista e azzera il puntatore alla lista
void free_users(struct users **lista){
    struct users *attuale = *lista;
    struct users *next = NULL;

    // Libera la memoria per tutti i nodi nella lista
    while(attuale != NULL){
        next = attuale->next;
        if(attuale->username != NULL)
            free(attuale->username);
        free(attuale);
        attuale = next;
    }

    // Azzera il puntatore alla lista
    *lista = NULL;
}

// Verifica se un processo con il dato PID è in esecuzione
int is_process_running(pid_t pid){
    int status;
    return waitpid(pid, &status, WNOHANG) == 0; // Restituisce 1 se il processo è in esecuzione, altrimenti 0
}

// Invia un segnale di risveglio al processo con il dato PID se è in esecuzione
int wake_up_process(pid_t pid){
    // Verifica se il processo è in esecuzione
    if(is_process_running(pid) == 1){
        // Invia il segnale di risveglio
        if(kill(pid, SIGUSR1) == -1){
            perror("Main Server: Errore nell'invio del segnale");
            return -1;
        }
    }
    else
        return 1;

    return 0;
}

// Game Server

int start_game_server(struct server_descriptor* s, const int port, const int room){
    int i = 1, listener, ret;
    struct sockaddr_in server_address;

    if(port < MIN_ACCESSIBLE_PORT){
        printf("Porta Privilegiata. Non è possibile utilizzarla. \n");
        return -1;
    }   
        
    // Controlla se la porta è già in uso da un altro Game Server
    if(search_room(s, port) > 0){
        printf("Porta non disponibile \n");
        return -1;
    }
    
    // Cerca un'entrata libera nella struttura del server
    i = search_room(s, 0);

    // Se i == -1 significa che sono pieni
    if(i == -1){
        printf("Sono stati registrati troppi Game Server \n");
        return -1;
    }
    
    // Crea e collega un socket sulla porta specificata
    listener = create_and_bind_socket(port, &server_address, 0);
    if(listener < 0)
        return -1;
    
    // Inizializza i dettagli del Game Server nella struttura server_descriptor
    s[i].port = port;
    s[i].sockfd = listener;
    ret = initialize_escape_room(&s[i].escape_room, room);
    if(ret < 0)
        return ret;

    printf("Avvio il Game Server %d sulla room (porta) %d ed implementa la %s ... \n", i, port, s[i].escape_room.theme);

    return i; // Restituisce l'indice del server appena avviato nella struttura server_descriptor
}

// Funzione che implementa il gioco in sé per sé
int game_escape_room(struct server_descriptor* s, const int index, const int room){
    int partita = 0; // Se è 0 indica che la partita non è iniziata, quando è 1 indica che la partita è iniziata
    int sd = s[index].sockfd, ret, newfd, codice = 0, i, j, indice_giocatore = -1, presenti = 0;
    struct player_descriptor players[MAX_CLIENTS];

    struct itimerval timer;
    
    struct sockaddr_in client_address; 
    socklen_t addrlen = sizeof(struct sockaddr_in);

    struct message* messages = NULL; // Lista di messaggi

    fd_set master; // Set di descrittori da monitorare    
    fd_set read_fds; // Set dei descrittori pronti    
    int fdmax; // Descrittore massimo

    memset(buffer, 0, sizeof(buffer));

    FD_ZERO(&master); wait(NULL);
    FD_ZERO(&read_fds);

    // Inizializza players
    initialize_players_descriptor(players);

    // Inizializza il timer
    set_timer(&timer, 0);

    // Imposta il gestore per il segnale di scadenza del timer
    if(signal(SIGALRM, timer_handler) == SIG_ERR){
        printf("Game Server %d: ", index);
        perror("Errore nell'impostare il gestore del timer");
        exit(EXIT_FAILURE);
    }

    // Imposta il gestore per il segnale che serve a mettere interrupted a 1 e segnalare che il Main Server ha cercato di interrompere il Game Server
    if(signal(SIGUSR1, signal_handler) == SIG_ERR){
        printf("Game Server %d: ", index);
        perror("Errore nell'impostare il gestore di segnali");
        exit(EXIT_FAILURE);
    }

    ret = listen(sd, MAX_CLIENTS);
    printf("Game Server %d attivo \n\n", index);

    // Aggiungo il socket di ascolto 'listener' e l'input da tastiera ai descrittori da monitorare
    FD_SET(sd, &master); 
    
    // Tengo traccia del nuovo fdmax
    fdmax = sd;
    
    while(1){     
        // Se tutti i giocatori sono pronti e la parttia non è cominciata fa partire immediatamente la partita   
        if(partita == 0 && control_ready_players(players) == MAX_CLIENTS){
            reset_timer(&timer, 0);
            timer_expired = 1;
        }

        // Se la partita non è cominciata e il timer è scaduto allora inizializza tutto affinché sia pronto per il gioco
        if(partita == 0 && timer_expired == 1){
            codice = 0; 
            win = 0; // La variabile globale win viene posta a 0

            // Se la Escape Room era già stata inizializzata la ricostruisce
            destroy_escape_room(&s[index].escape_room);
            ret = create_escape_room(&s[index].escape_room, room);
            if(ret < 0)
                return ret;

            // Se ci sono dei messaggi pendenti vengono tolti
            if(messages != NULL)
                destroy_message_list(&messages);

            // Scorro il player_descriptor players
            for(i = 0; i < MAX_CLIENTS; i++){
                // Se ci sono giocatori non pronti, li rendo pronti
                if(players[i].is_set == 1 && players[i].ready == 0){
                    printf("Game Server %d: %s è pronto! \n", index, players[i].username);
                    players[i].ready = 1;
                }
                
                // Invio le informazioni rilevanti ai giocatori effettivamente collegati col Game Server
                if(players[i].is_set == 1){
                    ret = send_all(players[i].fd, &codice, sizeof(codice), 0);
                    if(ret < 0){
                        players[i].is_set = 0;
                        players[i].ready = 0;
                        players[i].points = 0;
                        if(players[i].username != NULL){
                            free(players[i].username);
                            players[i].username = NULL;
                        }
                        close(players[i].fd);
                        players[i].fd = 0;
                        FD_CLR(players[i].fd, &master);
                        servers[index].clients--;
                    }
                    players[i].location = 0; // Metto il giocatore alla prima location

                    // Se aveva ancora oggetti nell'inventario vengono tolti
                    for(j = 0; j < players[i].num_items; j++)
                        destroy_object(&players[i].items[j]); 
                    players[i].num_items = 0;

                    // Mando le informazioni sulla prima location
                    // Prima il nome della prima location
                    ret = send_msg(players[i].fd, servers[index].escape_room.rooms[0].name, strlen(servers[index].escape_room.rooms[0].name), 0);
                    if(ret < 0){
                        players[i].is_set = 0;
                        players[i].ready = 0;
                        players[i].points = 0;
                        if(players[i].username != NULL){
                            free(players[i].username);
                            players[i].username = NULL;
                        }
                        close(players[i].fd);
                        players[i].fd = 0;
                        FD_CLR(players[i].fd, &master);
                        servers[index].clients--;
                    }

                    // Poi la descrizione della seconda location
                    ret = send_msg(players[i].fd, servers[index].escape_room.rooms[0].description, strlen(servers[index].escape_room.rooms[0].description), 0);
                    if(ret < 0){
                        players[i].is_set = 0;
                        players[i].ready = 0;
                        players[i].points = 0;
                        if(players[i].username != NULL){
                            free(players[i].username);
                            players[i].username = NULL;
                        }
                        close(players[i].fd);
                        players[i].fd = 0;
                        FD_CLR(players[i].fd, &master);
                        servers[index].clients--;
                    }
                }
            }

            printf("Game Server %d: Il gioco è partito... \n", index);
            timer_expired = 0; // Il timer viene resettato
            partita = 1; // La partita parte
            set_timer(&timer, TIME); // La partita dura TIME secondi
            setitimer(ITIMER_REAL, &timer, NULL); // Il timer viene attivato
        }


        // Imposto il set di socket da monitorare in lettura per la select()
        read_fds = master;   

        // Mi blocco (potenzialmente) in attesa di descrittori pronti. Attesa ***senza timeout*** (ultimo parametro attuale 'NULL')
        ret = select(fdmax + 1, &read_fds, NULL, NULL, NULL); 
        // Se ret < 0 qualcosa è andato storto
        if(ret < 0 && timer_expired == 1){
            // In questo caso è scaduto il timer. Niente di che. 
            interrupted = 0;
            continue;
        }
        else if(ret < 0 && partita == 0){
            // In questo caso il Main Server ha mandato un segnale per bloccare il Game Server. Và chiuso il Game Server perché la partita non è ancora iniziata. 
            printf("Game Server %d: Arrivo del segnale di arresto \n", index);
            if(timer.it_value.tv_sec > 0)
                reset_timer(&timer, 0);     
            break;
        }
        else if(ret < 0 && partita != 0){
            // In questo caso il Main Server ha mandato un segnale per bloccare il Game Server. Tuttavia la partita è iniziata e non si può chiudere il Main Server. 
            interrupted = 0;
            continue;
        }

        // Scorro ogni descrittore 'i'
        for(i = 0; i <= fdmax; i++){
            // Se il descrittore 'i' è rimasto nel set 'read_fds', cioè se la select() ce lo ha lasciato, allora 'i' è pronto
            if(FD_ISSET(i, &read_fds)){
                // Se c'è un'attività sul socket di ascolto, accetta una nuova connessione
                if(FD_ISSET(sd, &read_fds)){
                    fflush(stdout);
                    
                    // La partita non è ancora iniziata
                    if(partita == 0){
                        printf("Game Server %d: Client rilevato! \n", index);

                        reset_timer(&timer, 0);                                
                        
                        // Calcolo la lunghezza dell'indirizzo del Client
                        addrlen = sizeof(client_address);
                        // Accetto la connessione e creo il socket connesso "newfd"
                        newfd = accept(sd, (struct sockaddr *)&client_address, &addrlen);
                        if(newfd < 0){
                            printf("Game Server %d: ", index);
                            perror("Errore nell'accettazione di una richiesta");
                            continue;
                        }

                        // Si manda una presentazione al Client e si tiene di conto nella struttura player_descriptor players
                        ret = presentation(players, newfd, index, partita);
                        if(ret < 0){
                            close(newfd);
                            continue;
                        }
                        
                        FD_SET(newfd, &master);
                        // Aggiorno l'ID del massimo descrittore
                        if(newfd > fdmax)
                            fdmax = newfd;

                        servers[index].clients++;
                    }
                    // La partita è iniziata e la nuova connessione è automaticamente rifiutata
                    else{
                        // Accetto la connessione ...
                        newfd = accept(sd, (struct sockaddr *)&client_address, &addrlen);
                        if(newfd < 0)
                            continue;

                        // ... per comunicare che non è possibile giocare in questo momento perché una partita è attiva
                        ret = presentation(players, newfd, index, partita);
                        if(ret < 0){
                            close(newfd);
                            continue;
                        }
                        close(newfd);
                    }   
                }                
                // Altrimenti è la richiesta di un Client già connesso
                else{
                    fflush(stdout);
                    memset(buffer, 0, sizeof(buffer));

                    // A seconda del socket di comunicazione cerco chi è il player con cui ho a che fare
                    indice_giocatore = search_player(players, i);
                    if(indice_giocatore < 0){
                        close(i);
                        FD_CLR(i, &master);
                        continue;
                    }
                    
                    // La partita non è ancora iniziata
                    if(partita == 0){
                        // Ricevo un messaggio che mi dice che intenzioni ha il Client
                        ret = recv_msg(i, buffer, 0);
                        if(ret < 0){
                            close(i);
                            FD_CLR(i, &master);
                            players[indice_giocatore].is_set = 0;
                            players[indice_giocatore].ready = 0;
                            players[indice_giocatore].points = 0;
                            if(players[indice_giocatore].username != NULL){
                                free(players[indice_giocatore].username);
                                players[indice_giocatore].username = NULL;
                            }
                            players[indice_giocatore].fd = 0;
                            servers[index].clients--;
                            continue;
                        }

                        if(strncmp(buffer, "begin", 5) == 0){     
                            // Se ha inserito "begin" siginifica che è pronto a giocare
                            // Pulisco il buffer
                            memset(buffer, 0, strlen(buffer)+1);    
                            // Chiamo la funzione "begin"                 
                            ret = begin(i, index, &timer, players, indice_giocatore);
                            if(ret < 0){
                                // Caso di errore
                                close(i);
                                FD_CLR(i, &master);
                                players[indice_giocatore].is_set = 0;
                                players[indice_giocatore].ready = 0;
                                players[indice_giocatore].points = 0;
                                if(players[indice_giocatore].username != NULL){
                                    free(players[indice_giocatore].username);
                                    players[indice_giocatore].username = NULL;
                                }
                                players[indice_giocatore].fd = 0;
                                servers[index].clients--;
                                continue;
                            }
                            // In questo caso il Client era da solo e ha deciso di giocare in Single Player
                            else if(ret == 1)
                                break;
                        }
                        else if(strncmp(buffer, "quit", 4) == 0){   
                            // Se ha inserito "quit" siginifica che se ne vuole andare
                            // Pulisco il buffer
                            memset(buffer, 0, strlen(buffer)+1);   
                            // Chiamo la funzione "quit"                      
                            ret = quit(i, index, players, indice_giocatore, &timer);
                            // Chiudo la connessione
                            close(i);
                            // Lo tolgo dai descrittori
                            FD_CLR(i, &master);
                            // Indico che abbiamo un Client in meno
                            servers[index].clients--;
                            // Gestisce la struttura player_descriptor players
                            free(players[indice_giocatore].username);
                            players[indice_giocatore].is_set = 0;
                            continue;
                        }
                        else{
                            // In questo caso non è stata inviata una stringa valida
                            memset(buffer, 0, strlen(buffer)+1);
                            codice = -1;
                            ret = send_all(i, &codice, sizeof(codice), 0);
                            if(ret < 0){
                                perror("Errore nell'invio del codice");
                                close(i);
                                FD_CLR(i, &master);
                                players[indice_giocatore].is_set = 0;
                                players[indice_giocatore].ready = 0;
                                players[indice_giocatore].points = 0;
                                if(players[indice_giocatore].username != NULL){
                                    free(players[indice_giocatore].username);
                                    players[indice_giocatore].username = NULL;
                                }
                                players[indice_giocatore].fd = 0;
                                servers[index].clients--;
                                continue;
                            }
                        }
                    }
                    // La partita è iniziata
                    else{
                        // Il Client invia un codice quando è pronto a comunicare
                        ret = recv_all(i, &codice, sizeof(codice), 0);
                        if(ret < 0){
                            perror("Errore nella ricezione del codice di presentazione");
                            close(i);
                            FD_CLR(i, &master);
                            players[indice_giocatore].is_set = 0;
                            players[indice_giocatore].ready = 0;
                            players[indice_giocatore].points = 0;
                            if(players[indice_giocatore].username != NULL){
                                free(players[indice_giocatore].username);
                                players[indice_giocatore].username = NULL;
                            }
                            players[indice_giocatore].fd = 0;
                            servers[index].clients--;
                            timer_expired = 1;
                            reset_timer(&timer, 0);
                            continue;
                        }

                        // Se il codice è negativo significa che c'è stato un errore
                        if(codice < 0){
                            close(i);
                            FD_CLR(i, &master);
                            players[indice_giocatore].is_set = 0;
                            players[indice_giocatore].ready = 0;
                            players[indice_giocatore].points = 0;
                            if(players[indice_giocatore].username != NULL){
                                free(players[indice_giocatore].username);
                                players[indice_giocatore].username = NULL;
                            }
                            players[indice_giocatore].fd = 0;
                            servers[index].clients--;
                            timer_expired = 1;
                            reset_timer(&timer, 0);
                            continue;
                        }
                        
                        if((win > 0 || timer_expired > 0) && players[indice_giocatore].ready == 1){
                            // Se win è maggiore di 0 o timer_expired è maggiore di 0 significa che la partita in qualche modo è finita.
                            // Guarda se il player è ancora pronto (quindi non aspetta che gli altri escano dalla partita). 
                            // Il Client viene informato della fine della partita tramite un codice = 200.
                            codice = 200;
                            ret = send_all(i, &codice, sizeof(codice), 0);
                            if(ret < 0){
                                perror("Errore nell'invio del codice di stato");
                                close(i);
                                FD_CLR(i, &master);
                                players[indice_giocatore].is_set = 0;
                                players[indice_giocatore].ready = 0;
                                players[indice_giocatore].points = 0;
                                if(players[indice_giocatore].username != NULL){
                                    free(players[indice_giocatore].username);
                                    players[indice_giocatore].username = NULL;
                                }
                                players[indice_giocatore].fd = 0;
                                servers[index].clients--;
                                continue;
                            }

                            // Il Client viene informato sulla sua vittoria. win è 0 se ha perso, win è 1 se ha vinto
                            ret = send_all(i, &win, sizeof(win), 0);
                            if(ret < 0){
                                perror("Errore nell'invio del codice di stato");
                                close(i);
                                FD_CLR(i, &master);
                                players[indice_giocatore].is_set = 0;
                                players[indice_giocatore].ready = 0;
                                players[indice_giocatore].points = 0;
                                if(players[indice_giocatore].username != NULL){
                                    free(players[indice_giocatore].username);
                                    players[indice_giocatore].username = NULL;
                                }
                                players[indice_giocatore].fd = 0;
                                servers[index].clients--;
                                continue;
                            }

                            // Si inviano gli ultimi messaggi rimasti
                            ret = control_messages(players[indice_giocatore].fd, &messages, indice_giocatore, players);

                            // Il giocatore non è più pronto e aspetta che gli altri siano pronti
                            players[indice_giocatore].ready = 0;
                        }
                        else if(players[indice_giocatore].ready == 1){
                            // Se la partita non è finita, guarda che il giocatore sia pronto. 
                            // Gli viene inviato un codice di conferma sulla prosecuzione della partita.
                            codice = 0;
                            ret = send_all(i, &codice, sizeof(codice), 0);
                            if(ret < 0){
                                perror("Errore nell'invio del codice di stato");
                                close(i);
                                FD_CLR(i, &master);
                                players[indice_giocatore].is_set = 0;
                                players[indice_giocatore].ready = 0;
                                players[indice_giocatore].points = 0;
                                if(players[indice_giocatore].username != NULL){
                                    free(players[indice_giocatore].username);
                                    players[indice_giocatore].username = NULL;
                                }
                                players[indice_giocatore].fd = 0;
                                servers[index].clients--;
                                timer_expired = 1;
                                reset_timer(&timer, 0);
                                continue;
                            }

                            // Chiama la funzione "handle_client_commands" per gestire i comandi che invierà il Client
                            ret = handle_client_commands(i, servers, index, players, indice_giocatore, &messages, &timer);
                            if(ret < 0){
                                close(i);
                                FD_CLR(i, &master);
                                players[indice_giocatore].is_set = 0;
                                players[indice_giocatore].ready = 0;
                                players[indice_giocatore].points = 0;
                                if(players[indice_giocatore].username != NULL){
                                    free(players[indice_giocatore].username);
                                    players[indice_giocatore].username = NULL;
                                }
                                players[indice_giocatore].fd = 0;
                                servers[index].clients--;
                                timer_expired = 1;
                                reset_timer(&timer, 0);
                                continue;
                            }
                        }                        
                        
                        // Guarda quanti giocatori ci sono ancora con ready = 1
                        presenti = control_ready_players(players);

                        if(presenti <= 0){
                            // Se non c'è nessun giocatore con ready = 1, la partita finisce
                            for(j = 0; j < MAX_CLIENTS; j++){
                                // Se il player è riconosciuto nella struttura gli vengono inviate le ultime informazioni
                                if(players[j].is_set == 1 && players[j].ready == 0){
                                    // Si invia il contenuto di "partita" per indicare che la partita è finita
                                    ret = send_all(players[j].fd, &partita, sizeof(partita), 0);
                                    if(ret < 0){
                                        perror("Errore nell'invio del codice di fine partita");
                                        close(players[j].fd);
                                        players[j].fd = 0;
                                        players[j].is_set = 0;
                                        players[j].ready = 0;
                                        players[j].points = 0;
                                        FD_CLR(players[j].fd, &master);
                                        servers[index].clients--;
                                        close(i);
                                        if(players[j].username != NULL){
                                            free(players[j].username);
                                            players[j].username = NULL;
                                        }
                                        players[j].is_set = 0;
                                        reset_timer(&timer, 0);
                                        continue;
                                    }
                                }
                            }
                            if(win == 0)
                                printf("Game Server %d: La partita si è conclusa. I giocatori hanno perso. \n", index);
                            else
                                printf("Game Server %d: La partita si è conclusa. I giocatori hanno vinto. \n", index);

                            // Viene resettato lo stato del Server
                            partita = 0; // La partita è finita e quindi si ritorna alla fase dove si aspetta che i giocatori siano pronti
                            timer_expired = 0; // Il flag timer_expired viene messo a 0
                            win = 0; // La condizione di vittoria viene messa a 0 in ogni caso
                            reset_timer(&timer, 0); // Il timer è resettato per non farlo proseguire
                            
                        }
                    }
                }
            }
        } 

        // Nel caso non vi siano Client è bene confermare che la variabile locale "partita" e la variabile globale "timer_expired" siano a 0
        if(servers[index].clients <= 0){
            partita = 0;
            timer_expired = 0;
        }       
    }

    // Si chiudono tutte le connessioni tranne quella col socket di ascolto
    for(i = 0; i <= fdmax; i++){
        if(FD_ISSET(i, &master) && i != sd)
            close(i);
    }

    // Si distruggono i messaggi se ne sono rimasti
    if(messages != NULL)
        destroy_message_list(&messages);

    // Si chiude il Game Server
    close_game_server(sd, index);

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    exit(EXIT_SUCCESS);
}


void close_game_server(const int listener, const int indice){
    printf("Main Server: Chiudo il Game Server %d \n", indice);
    close(listener);

    destroy_escape_room(&servers[indice].escape_room);
}


void signal_handler(const int signal){
    if(signal == SIGUSR1)
        interrupted = 1;
}


void timer_handler(int signal){
    if(signal == SIGALRM){
        interrupted = 0;
        timer_expired = 1;
        printf("Tempo scaduto \n");
    }
}


void initialize_players_descriptor(struct player_descriptor* p){
    int i = 0;
    for(; i < MAX_CLIENTS; i++){
        p[i].is_set = 0;
        p[i].fd = -1;
        p[i].username = NULL;
        p[i].ready = 0;
        p[i].points = 0;
        p[i].num_items = 0;
        p[i].location = -1;
    }
}

// Setta il timer al tempo "time"
void set_timer(struct itimerval* timer, const int time){
    if(time < 0)
        return;

    timer->it_value.tv_sec = time; // Scatta dopo "time" secondi
    timer->it_value.tv_usec = 0;
    timer->it_interval.tv_sec = 0; // Non ci sono timer ripetuti
    timer->it_interval.tv_usec = 0;
}

// Resetta il timer
void reset_timer(struct itimerval* timer, const int time){
    if(time < 0)
        return;

    getitimer(ITIMER_REAL, timer);
    if(timer->it_value.tv_sec != 0 || timer->it_value.tv_usec != 0){
        // Il timer è attivo e in esecuzione e quindi va resettato
        set_timer(timer, time);
        if(time > 0)
            setitimer(ITIMER_REAL, timer, NULL);
    }
}

// La funzione accetta un puntatore a una struttura player_descriptor, un descrittore di socket (sockfd), il numero del Game Server e uno stato di gioco
int presentation(struct player_descriptor* p, const int sockfd, const int index, const int game){
    int ret, i = 0; 

    // Invia lo stato della partita al client
    ret = send_all(sockfd, &game, sizeof(game), 0);
    if(ret < 0){
        printf("Game Server %d: ", index);
        perror("Errore nell'invio dello stato della partita");
        return ret;
    }

    // Invia il numero del Game Server al Client
    ret = send_all(sockfd, &index, sizeof(index), 0);
    if(ret < 0){
        printf("Game Server %d: ", index);
        perror("Errore nell'invio dell'ID");
        return ret;
    }

    // Se game non è 0 significa che la partita è attiva e che quindi la connessione è automaticamente rifiutata
    if(game != 0)
        return game; 

    // Riceve lo username del Client
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0){
        printf("Game Server %d: ", index);
        perror("Errore nella ricezione dello username");
        return ret;
    }

    // Controlla quanti posti sono liberi
    for(; i < MAX_CLIENTS; i++){
        if(p[i].is_set == 0)
            break;
    }

    // Se non ci sono posti liberi esce
    if(i == MAX_CLIENTS){
        printf("Game Server %d: L'utente %s NON si è collegato. Al momento il Server ha raggiunto il massimo di giocatori. \n", index, buffer);
        return -1;
    }

    // Nel caso sia rimasto lo username precedente lo toglie. Non dovrebbe mai succedere. 
    if(p[i].username != NULL)
        free(p[i].username);
    
    // Alloca lo username e controlla non ci siano errori nell'allocazione
    p[i].username = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(p[i].username == NULL){
        printf("Game Server %d: Errore nella memorizzazione dello username \n", index);
        memset(buffer, 0, ret+1);
        return -1;
    }
    strcpy(p[i].username, buffer);
    
    p[i].is_set = 1; // Viene settato a 1 per dire che nel player_descriptor questo 'posto' è preso
    p[i].fd = sockfd; // Viene memorizzato il socket per i messaggi in broadcasting e per la ricerca del player in base al socket
    memset(buffer, 0, ret+1);

    printf("Game Server %d: L'utente %s si è collegato \n", index, p[i].username);
    
    return 0;
}


// La funzione search_player cerca un giocatore nella struttura player_descriptor basandosi sul socket descriptor (sockfd)
int search_player(struct player_descriptor* p, const int sockfd){
    int i = 0, risultato = -1;

    for(; i < MAX_CLIENTS; i++){
        if(p[i].is_set == 1 && p[i].fd == sockfd){
            risultato = i;
            break;
        }
    }

    // Restituisce l'indice del giocatore se trovato, altrimenti -1
    return risultato;
}

// La funzione control_set_players conta il numero di giocatori settati nella struttura player_descriptor
int control_set_players(struct player_descriptor* p){
    int i = 0, giocatori_pronti = 0;
    
    for(; i < MAX_CLIENTS; i++){
        if(p[i].is_set == 1)
            giocatori_pronti++;
    }

    return giocatori_pronti;
}

// La funzione control_ready_players conta il numero di giocatori pronti nella struttura player_descriptor
int control_ready_players(struct player_descriptor* p){
    int i = 0, giocatori_pronti = 0;
    
    for(; i < MAX_CLIENTS; i++){
        if(p[i].is_set == 1 && p[i].ready == 1)
            giocatori_pronti++;
    }

    return giocatori_pronti;
}

// La funzione is_numeric verifica se una stringa contiene solo caratteri numerici
int is_numeric(const char* string){
    int i = 0, lunghezza; 

    if(string == NULL)
        return 0;

    lunghezza = strlen(string); 

    for(; i < lunghezza; i++){
        if(string[i] < '0' || string[i] > '9')
            return 0; // Se un carattere non è un numero, restituisce falso
    }

    // Restituisce 1 se la stringa è composta solo da numeri, altrimenti restituisce 0
    return 1; // Se tutti i caratteri sono numeri, restituisce vero
}

// La funzione count_points conta la somma totale dei punti dei giocatori nella struttura player_descriptor
int count_points(struct player_descriptor* p){
    int i = 0, punti = 0;

    for(; i < MAX_CLIENTS; i++){
        if(p[i].is_set == 1)
            punti += p[i].points; 
    }

    return punti;
}

// La funzione send_object invia le informazioni sull'oggetto attraverso il socket specificato
int send_object(const int sockfd, const struct object* o){
    int ret, i = 0, codice;

    // Invia il nome dell'oggetto al Client e verifica gli errori
    ret = send_msg(sockfd, o->name, strlen(o->name), 0);
    if(ret < 0)
        return ret;

    // Riceve un codice dal Client per verificare che sia stato allocato correttamente lo spazio per contenere il nome
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }

    // Se il codice è negativo, restituisce il codice come errore
    if(codice < 0)
        return codice;

    // Invia la descrizione dell'oggetto al Client e verifica gli errori
    ret = send_msg(sockfd, o->description, strlen(o->description), 0);
    if(ret < 0)
        return ret;

    // Riceve un codice dal Client per verificare che sia stato allocato correttamente lo spazio per contenere la descrizione
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }

    // Se il codice è negativo, restituisce il codice come errore
    if(codice < 0)
        return codice;

    // Invia il tipo dell'oggetto al Client
    ret = send_all(sockfd, &o->type, sizeof(o->type), 0);
    if(ret < 0){
        perror("Errore nell'invio del tipo dell'oggetto");
        return ret;
    }

    // Invia il tipo dell'enigma associato all'oggetto al Client
    ret = send_all(sockfd, &o->enigma.type, sizeof(o->enigma.type), 0);
    if(ret < 0){
        perror("Errore nell'invio del tipo dell'enigma");
        return ret;
    }

    // Invia la domanda dell'enigma associato all'oggetto al Client
    ret = send_msg(sockfd, o->enigma.question, strlen(o->enigma.question), 0);
    if(ret < 0)
        return ret;

    // Riceve un codice dal Client per verificare che sia stato allocato correttamente lo spazio per contenere la domanda dell'enigma
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }

    // Se il codice è negativo, restituisce il codice come errore
    if(codice < 0)
        return codice;

    // Invia la soluzione dell'enigma associato all'oggetto al Client
    ret = send_msg(sockfd, o->enigma.solution, strlen(o->enigma.solution), 0);
    if(ret < 0)
        return ret;

    // Riceve un codice dal Client per verificare che sia stato allocato correttamente lo spazio per contenere la soluzione dell'enigma
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }

    // Se il codice è negativo, restituisce il codice come errore
    if(codice < 0)
        return codice;

     // Se l'enigma è a scelta multipla, invia le opzioni al Client
    if(o->enigma.type == 1){
        for(; i < 4; i++){
            ret = send_msg(sockfd, o->enigma.options[i], strlen(o->enigma.options[i]), 0);
            if(ret < 0)
                return ret;

            ret = recv_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nella ricezione del codice");
                return ret;
            }

            // Se il codice è negativo, restituisce il codice come errore
            if(codice < 0)
                return codice;
        }

        // Invia il numero di tentativi rimasti al Client
        ret = send_all(sockfd, &o->enigma.attempts_left, sizeof(o->enigma.attempts_left), 0);
        if(ret < 0){
            perror("Errore nell'invio del tipo dell'enigma");
            return ret;
        }
    }    

    return 0;
}

// Funzione per la creazione di un nuovo messaggio con i dati forniti
struct message* create_message(const char* phrase, const char* username, const int location, const int giocatore, const int tipo){
    struct message* new_message = (struct message*) malloc(sizeof(struct message));
    int i;

    // Imposta il tipo del messaggio come 1 se non specificato correttamente
    if(tipo < 0 || tipo > 1)
        new_message->type = 1;
    else 
        new_message->type = tipo;

    // Caso in cui non ci sia spazio sufficiente in memoria
    if(new_message == NULL){
        printf("Errore nella creazione del messaggio: Spazio insufficiente! \n");
        return NULL;
    }

    // Caso in cui l'indice del giocatore sia errato
    if(giocatore < 0 || giocatore >= MAX_CLIENTS){
        printf("Errore nella creazione del messaggio: L'indice del giocatore è errato! \n");
        free(new_message);
        return NULL;
    }

    // Inizializza gli attributi del messaggio
    new_message->phrase = strdup(phrase);
    new_message->username = strdup(username);
    new_message->location = location;

    for(i = 0; i < MAX_CLIENTS; i++){
        if(i == giocatore)
            new_message->views[i] = 1;
        else
            new_message->views[i] = 0;
    }

    new_message->next = NULL;

    return new_message;
}

// Aggiunge un messaggio alla fine della lista
void add_message(struct message** head, const char* phrase, const char* username, const int location, const int giocatore, const int type){
    struct message* new_message = create_message(phrase, username, location, giocatore, type);
    struct message* current = *head;
    
    // Gestisce il caso in cui l'indice del giocatore è errato
    if(giocatore < 0 || giocatore >= MAX_CLIENTS){
        printf("Errore nell'aggiunta del messaggio: Indice del giocatore errato! \n");
        return;
    }

    // Aggiunge il nuovo messaggio alla fine della lista
    if(current == NULL)
        *head = new_message;
    else{
        while(current->next != NULL)
            current = current->next;
                
        current->next = new_message;
        printf("Messaggio inserito! \n");
    }
}

// Dealloca la memoria associata a un singolo messaggio
void destroy_message(struct message* msg){
    if(msg != NULL){
        free(msg->phrase);
        free(msg->username);
        free(msg);
    }
}

// Rimuove un messaggio specifico dalla lista
void remove_message(struct message** head, struct message* target){
    struct message* current = *head;
    struct message* successivo = (*head)->next;

    // Caso in cui la lista sia vuota
    if(*head == NULL){
        printf("Errore nella rimozione del messaggio: Lista vuota! \n");
        return;
    }    

    // Rimuove il primo messaggio se è il target
    if(current == target){
        destroy_message(current);
        *head = successivo;
        return;
    }

    // Ricerca e rimuove il messaggio target dalla lista
    while(current != NULL && current->next != target)
        current = current->next;

    if(current != NULL){
        current->next = target->next;
        destroy_message(target);
    }
}

// Dealloca tutta la lista di messaggi e imposta il puntatore alla testa a NULL
void destroy_message_list(struct message** head){
    while(*head != NULL){
        struct message* next = (*head)->next;
        destroy_message(*head);
        *head = next;
    }
}

// Conta il numero di messaggi non letti per un giocatore specifico
int count_pending_messages(const struct message* head, const int giocatore){
    const struct message* current = head;
    int conteggio = 0;

    // Caso in cui l'indice del giocatore è errato.
    if(giocatore < 0 || giocatore >= MAX_CLIENTS)
        return -1;

    // Conta i messaggi non letti per il giocatore specifico.
    while(current != NULL){
        if(current->views[giocatore] == 0)
            conteggio++;
        current = current->next;
    }

    return conteggio;
}

// La funzione control_messages gestisce l'invio dei messaggi pendenti al Client
int control_messages(const int sockfd, struct message** head, const int giocatore, struct player_descriptor* p){
    struct message* current = *head;
    struct message* previous = NULL;
    int ret, numero_giocatori = 0, i = 0, codice = 0, conteggio = 0;

    if(head == NULL || giocatore < 0 || giocatore >= MAX_CLIENTS){
        // Input non valido
        return -1;
    }

    // Conta il numero di giocatori pronti nel gioco
    for(; i < MAX_CLIENTS; i++){
        if(p[i].is_set == 1 && p[i].ready == 1)
            numero_giocatori++;
    }

    // Itera sulla lista dei messaggi
    while(current != NULL){
        // Se il giocatore non ha ancora visto il messaggio entra
        if(current->views[giocatore] == 0){
            // Nel caso sia nella stessa location dove è presente il messaggio, gli viene inviato. La location del giocatore deve essere lo stesso dove è stato 
            // inviato il messaggio. Se la location del messaggio è -1 significa che lo devono ricevere tutti
            if(p[giocatore].location == current->location || current->location == -1){
                // Invia il codice al Client
                codice = 0;
                ret = send_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0){
                    perror("Errore nell'invio del codice");
                    return ret;
                }

                // Invia il nome dell'utente che ha mandato il messaggio al Client
                ret = send_msg(sockfd, current->username, strlen(current->username), 0);
                if(ret < 0)
                    return ret;

                // Riceve un codice dal Client per vedere se l'username è stato allocato correttamente
                ret = recv_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0){
                    perror("Errore nella ricezione del codice");
                    return ret;
                }

                // Se il codice è negativo esce
                if(codice < 0)
                    return codice;

                // Invia la frase del messaggio al Client
                ret = send_msg(sockfd, current->phrase, strlen(current->phrase), 0);
                if(ret < 0)
                    return ret;

                // Riceve un codice dal Client per vedere se la frase è stata allocata correttamente
                ret = recv_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0){
                    perror("Errore nella ricezione del codice");
                    return ret;
                }

                // Se il codice è negativo esce
                if(codice < 0)
                    return codice;    

                // / Invia il tipo del messaggio al Client
                ret = send_all(sockfd, &current->type, sizeof(current->type), 0);
                if(ret < 0){
                    perror("Errore nell'invio del tipo di messaggio");
                    return ret;
                }    
            }

            // In ogni caso segna il messaggio come visualizzato dal giocatore corrente, sia che gli sia stato inviato o meno
            current->views[giocatore] = 1;
        }

        // Conta quanti giocatori hanno visualizzato il messaggio
        conteggio = 0;
        for(i = 0; i < MAX_CLIENTS; i++)
            conteggio += current->views[i];

        // Se tutti i giocatori presenti hanno visualizzato il messaggio, lo rimuove dalla lista
        if(conteggio >= numero_giocatori){
            remove_message(head, current);
            if(previous == NULL)
                current = *head;
            else 
                current = previous->next;
        }
        else{
            previous = current;
            current = current->next;
        }
    }

    // Invia un codice di conferma al Client
    codice = 1;
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    memset(buffer, 0, sizeof(buffer));

    return 0; 
}

// Comandi

// Questa funzione gestisce i comandi del Client. A seconda del comando ricevuto, determina quale protocollo utilizzare in combinazione con il Client. 
int handle_client_commands(const int sockfd, struct server_descriptor* s, const int index, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, codice;
    char* comando;
    char* stringa;
    char* posizione;

    // Ricevi il comando dal Client
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0){
        memset(buffer, 0, sizeof(buffer));
        return ret;
    }   
    
    // Alloca dinamicamente la memoria per il comando e verifica se l'allocazione ha successo
    comando = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(comando == NULL){
        memset(buffer, 0, strlen(buffer) + 1);
        return ret;
    }
    strcpy(comando, buffer);
    if(strcmp(comando, "timeout") != 0) // Stampa il comando ricevuto (escludendo "timeout" per non inquinare l'output)
        printf("%s: %s\n", p[giocatore].username, comando);

    // Gestione dei comandi
    if(strncmp(comando, "goto ", 5) == 0){
        // Esegue il comando "goto"
        posizione = strstr(comando, "goto ");
        if(posizione == NULL){
            codice = -1;
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                printf("Game Server %d: Errore nell'invio del codice all'utente %s", index, p[giocatore].username);
                return ret;
            }
            return -1;
        }
        // Avanza il puntatore oltre "goto "
        posizione += 5;

        // Ottieni la stringa desiderata
        stringa = strdup(posizione);

        ret = go_to(sockfd, stringa, s, index, p, giocatore, message_list, timer);

        free(stringa);
        free(comando);
        return ret;
    }
    else if(strncmp(comando, "look", 4) == 0){
        // Esegue il comando "look"
        if(strcmp(comando, "look") == 0){
            // Se look si basa sulla location attuale
            stringa = NULL;
            ret = look(sockfd, stringa, s, index, p, giocatore, message_list, timer);
        }
        else if(strncmp(comando, "look ", 5) == 0 && strcmp(comando, "look ") != 0){
            // Se look si basa sulla descrizion di un oggetto
            posizione = strstr(comando, "look ");
            // Avanza il puntatore oltre "look "
            posizione += 5;

            // Ottieni la stringa desiderata
            stringa = strdup(posizione);

            if(stringa == NULL){
                // Errore
                codice = -1;
                ret = send_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0){
                    printf("Game Server %d: Errore nell'invio del codice all'utente %s", index, p[giocatore].username);
                    return ret;
                }
            }

            ret = look(sockfd, stringa, s, index, p, giocatore, message_list, timer);
            free(stringa);
        }
        else{
            // In caso di errore
            codice = -1;
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                printf("Game Server %d: Errore nell'invio del codice all'utente %s", index, p[giocatore].username);
                return ret;
            }
            ret = 0;
        }
            
        free(comando);
        return ret;
    }
    else if(strncmp(comando, "take ", 5) == 0){
        // Esegue il comando "take"
        posizione = strstr(comando, "take ");
        if(posizione == NULL){
            codice = -1;
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                printf("Game Server %d: Errore nell'invio del codice all'utente %s", index, p[giocatore].username);
                return ret;
            }
            return -1;
        }
        // Avanza il puntatore oltre "take "
        posizione += 5;

        // Ottieni la stringa desiderata
        stringa = strdup(posizione);

        ret = take(sockfd, stringa, s, index, p, giocatore, message_list, timer);

        free(stringa);
        free(comando);
        return ret;
    }
    else if(strncmp(comando, "interact ", 9) == 0){
        // Esegue il comando "take"
        posizione = strstr(comando, "interact ");
        if(posizione == NULL){
            codice = -1;
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                printf("Game Server %d: Errore nell'invio del codice all'utente %s", index, p[giocatore].username);
                return ret;
            }
            return -1;
        }
        // Avanza il puntatore oltre "interact "
        posizione += 9;

        // Ottieni la stringa desiderata
        stringa = strdup(posizione);

        ret = take(sockfd, stringa, s, index, p, giocatore, message_list, timer);

        free(stringa);
        free(comando);
        return ret;
    }
    else if(strcmp(comando, "go_on") == 0){
        // Esegue il "go_on"
        free(comando);
        return go_on(sockfd, s, index, p, giocatore, message_list, timer);
    }
    else if(strcmp(comando, "objs") == 0){
        // Esegue il comando "objs"
        free(comando);
        return objs(sockfd, p, giocatore, message_list, timer);
    }
    else if(strcmp(comando, "doors") == 0){
        // Esegue il comando "doors"
        free(comando);
        return doors(sockfd, s, index, p, giocatore, message_list, timer);
    }
    else if(strncmp(comando, "use ", 4) == 0){
        // Esegue il comando "use"
        stringa = (char*) malloc(strlen(comando) * sizeof(char));
        posizione = (char*) malloc(strlen(comando) * sizeof(char));
        ret = sscanf(comando, "use %s %s", stringa, posizione);
        // In stringa è contenuto "object1" e in posizione "object2"
        if(ret == 1){    
            // Se c'è un solo oggetto
            ret = use(sockfd, stringa, s, index, p, giocatore, message_list, timer);
        } 
        else if(ret == 2){
            // Se ci sono due oggetti
            ret = use_object(sockfd, stringa, posizione, s, index, p, giocatore, message_list, timer);
        }
        else{
            // Nessun oggetto o errore
            codice = -1;
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nell'invio del codice");
                return ret;
            }
            ret = 0;
        }
        free(stringa);
        free(posizione);
        return ret;   
    }
    else if(strncmp(comando, "message ", 8) == 0){
        // Esegue il comando "message"
        posizione = strstr(comando, "message ");
        if(posizione == NULL){
            codice = -1;
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                printf("Game Server %d: Errore nell'invio del codice all'utente %s", index, p[giocatore].username);
                return ret;
            }
            return -1;
        }
        // Avanza il puntatore oltre "message "
        posizione += 8;

        // Ottieni la stringa desiderata
        stringa = strdup(posizione);

        ret = message(sockfd, message_list, stringa, s, index, p, giocatore, timer);

        free(stringa);
        free(comando);
        return ret;
    }
    else if(strcmp(comando, "help") == 0) // Riceve il comando "help"
        return 0;
    else if(strcmp(comando, "time") == 0){
        // Esegue il comando "time"
        free(comando);
        return send_time(sockfd, p, giocatore, message_list, timer);
    }
    else if(strcmp(comando, "end") == 0){
        // Esegue il comando "end"
        free(comando);
        return end(sockfd, s, index, p, giocatore, message_list, timer);
    }
    else if(strcmp(comando, "timeout") == 0){
        // Esegue il "timeout"
        free(comando);
        return timeout(sockfd, message_list, p, giocatore);
    }
    else if(strcmp(comando, "none") == 0)
        return 0;        

    return -1; // Nel caso non gli sia arrivato nessuno di questi comandi restituisce un codice di errore, ma non dovrebbe mai succedere
}

// La funzione begin gestisce l'inizio di una partita nel Game Server. Controlla la condizione di prontezza dei giocatori e avvia il timer se necessario.
int begin(const int sockfd, const int index, struct itimerval* timer, struct player_descriptor* p, const int giocatore){
    int ret, codice = 0, pronti = 1;

    // Verifica se ci sono almeno 2 giocatori pronti
    if(servers[index].clients > 1 && servers[index].clients <= MAX_CLIENTS){
        p[giocatore].ready = 1;

        // Invia al giocatore un codice per indicare la corretta ricezione del comando begin
        codice = 0;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0)
            return ret;

        // Controlla il numero di giocatori pronti
        pronti = control_ready_players(p);

        // Se i giocatori sono pronti, avvia il timer
        if(pronti < control_set_players(p))
            printf("Game Server %d: %s è pronto! \n", index, p[giocatore].username);
        else{
            printf("Game Server %d: %s è pronto! \n", index, p[giocatore].username);
            set_timer(timer, WAITING);
            setitimer(ITIMER_REAL, timer, NULL);
            printf("Game Server %d: Partenza timer di %d secondi \n", index, WAITING);
        }
    }
    // Se c'è solo un giocatore, aspetta l'arrivo di altri giocatori o se il giocatore sceglie di giocare single-player
    else if(servers[index].clients == 1){
        p[giocatore].ready = 1;

        // Invia al giocatore l'informazione che è solo nel Game Server e che può decidere di giocare single-player
        codice = 1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0)
            return ret;
        
        // Riceve il codice indicante la scelta del giocatore
        ret = recv_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0)
            return ret;
        
        if(codice < 0) // Caso di Errore
            return codice;
        else if(codice == 0) // Caso in cui desidera giocare multi-player
            printf("Game Server %d: %s è pronto! \n", index, p[giocatore].username);
        else if(codice == 1){
            // Il giocatore giocherà in single-player
            printf("Game Server %d: %s è pronto! \n", index, p[giocatore].username);
            timer_expired = 1;
            return 1;
        }
    }
    else{
        // Caso di errore
        codice = -1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0)
            return ret;
    }

    return 0;
}

// La funzione quit gestisce l'uscita di un giocatore dal Game Server. Chiude la connessione, libera le risorse e aggiorna lo stato del giocatore.
int quit(const int sockfd, const int index, struct player_descriptor* p, const int giocatore, struct itimerval* timer){
    int ret, codice = 0, i = 0;

    // Invia al giocatore il codice di uscita
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        p[giocatore].is_set = 0;
        p[giocatore].fd = 0;
        p[giocatore].ready = 0;
        p[giocatore].points = 0;
        free(p[giocatore].username);
        p[giocatore].username = NULL;
        return ret;
    }

    // Stampa un messaggio di uscita del giocatore
    printf("Game Server %d: %s è uscito. \n", index, p[giocatore].username);

    // Distrugge gli oggetti posseduti dal giocatore se ne ha
    if(p[giocatore].num_items > 0){
        for(; i < p[giocatore].num_items; i++)
           destroy_object(&p[giocatore].items[i]);
        
        p[giocatore].num_items = 0;
    }
    
    // Aggiorna lo stato del giocatore
    p[giocatore].is_set = 0;
    p[giocatore].fd = 0;
    p[giocatore].ready = 0;
    p[giocatore].points = 0;
    free(p[giocatore].username);
    p[giocatore].username = NULL;

    // Questa parte serve nel caso tutti gli altri giocatori siano pronti
    if(control_set_players(p) > 0 && control_ready_players(p) == control_set_players(p)){
        set_timer(timer, WAITING);
        setitimer(ITIMER_REAL, timer, NULL);
        printf("Game Server %d: Partenza timer di %d secondi \n", index, WAITING);
    }

    return 0;
}

// Comando che permette al giocatore di spostarsi in un'altra location
int go_to(const int sockfd, const char* location, struct server_descriptor* s, const int index, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, i = 0, num_location, old_location, codice = -1;
    struct location* l = &s[index].escape_room.rooms[p[giocatore].location];
    struct location* r;
    const char* aggiunta = "È andato nella location "; // Messaggio di sistema da inviare agli altri giocatori
    char* stringa;

    // Verifica se la stringa della location è nulla
    if(location == NULL){
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            printf("Game Server %d: Errore nell'invio del codice all'utente %s", index, p[giocatore].username);
            return ret;
        }
        return -1;
    }   

     // Verifica se la stringa rappresenta un numero
    ret = is_numeric(location);
    if(ret == 1){
        // La stringa è un numero, quindi è il numero della porta
        num_location = atoi(location);
        // Verifica se il numero della location è valido
        if(num_location < 0 || num_location >= l->num_doors)
            codice = -1;
        else{
            // Verifica se la porta è aperta
            if(l->linked_locations[num_location].is_blocked == 0){
                codice = 0;
                old_location = p[giocatore].location;
                p[giocatore].location = l->linked_locations[num_location].next_location;

                // Aggiungi un messaggio di sistema alla lista di messaggi che informa gli altri giocatori di cosa è successo
                stringa = (char*) malloc((strlen(aggiunta) + strlen(location) + 1));
                if(stringa != NULL){
                    strcpy(stringa, aggiunta);
                    strcat(stringa, location);
                    add_message(message_list, stringa, p[giocatore].username, old_location, giocatore, 0);
                    free(stringa);
                }
            }
            else
                codice = 1; // La porta è chiusa
        }
    }
    else{
        // La stringa rappresenta un nome di location, quindi è una porta diretta
        for(; i < l->num_doors; i++){
            num_location = l->linked_locations[i].next_location;
            r = &s[index].escape_room.rooms[num_location];
            // Verifica se il nome della location corrisponde
            if(strcmp(location, r->name) == 0){
                // Verifica se la porta è aperta
                if(l->linked_locations[i].is_blocked == 0){
                    codice = 0;
                    old_location = p[giocatore].location;
                    p[giocatore].location = num_location;

                    // Aggiungi un messaggio di sistema alla lista di messaggi che informa gli altri giocatori di cosa è successo
                    stringa = (char*) malloc((strlen(aggiunta) + strlen(location) + 1));
                    if(stringa == NULL)
                        break;
                    strcpy(stringa, aggiunta);
                    strcat(stringa, location);
                    add_message(message_list, stringa, p[giocatore].username, old_location, giocatore, 0);
                    free(stringa);
                    break;
                }
                else{
                    codice = 1; // La porta è chiusa
                    break;
                }
            }
        }
    } 

    // Invia al giocatore il codice di risposta alla sua richiesta di spostamento. 
    // -1: Errore, 0: Spostamento avvenuto con successo, 1: Porta chiusa.
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        printf("Game Server %d: Errore nell'invio del codice all'utente %s", index, p[giocatore].username);
        return ret;
    }

    // Se il codice è 0 (spostamento avvenuto con successo), viene chiamata automaticamente il comando "look"
    if(codice == 0){
        ret = recv_msg(sockfd, buffer, 0);
        if(ret < 0)
            return ret;
        ret = look(sockfd, NULL, s, index, p, giocatore, message_list, timer);
    }
    else // Altrimenti si inviano direttamente le informazioni sul timer e sui punteggi
        ret = send_time(sockfd, p, giocatore, message_list, timer);

    return ret; // Restituisce il risultato dell'operazione di spostamento
}

// Comando che permette ad un giocatore di guardare cosa c'è nella location e di vedere la descrizione di un oggetto
int look(const int sockfd, const char* object, struct server_descriptor* s, const int index, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, i = 0, codice = 0;
    struct location* l = &s[index].escape_room.rooms[p[giocatore].location];

    if(object == NULL){
        // Se la stringa object è NULL significa che si cerca di guardare la location
        // Invia il codice 0 per indicare che la richiesta è valida
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice");
            return ret;
        }

        // Invia il nome della location attuale
        ret = send_msg(sockfd, l->name, strlen(l->name), 0);
        if(ret < 0)
            return ret;
        
        // Invia la descrizione della location attuale
        ret = send_msg(sockfd, l->description, strlen(l->description), 0);
        if(ret < 0)
            return ret;
        
        // Invia il codice 1 per indicare che verranno inviati ulteriori dettagli (giocatori presenti)
        codice = 1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice");
            return ret;
        }

        codice = 0; // Codice qua farà da contatore per indicare quante persone ci sono nella location
        for(; i < MAX_CLIENTS; i++){
            if(i != giocatore && p[i].is_set == 1 && p[i].location == p[giocatore].location)
                codice++;
        }

        // Invia al Client il numero di giocatori di cui riceverà lo username
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice");
            return ret;
        }

        if(codice > 0){
            for(i = 0; i < MAX_CLIENTS; i++){
                if(i != giocatore && p[i].is_set == 1 && p[i].location == p[giocatore].location){
                    // Gli invia lo username dei giocatori presenti nella stessa location
                    ret = send_msg(sockfd, p[i].username, strlen(p[i].username), 0);
                    if(ret < 0)
                        return ret;
                }
            }
        }
    }
    else{
        // Altrimenti si presuppone che nella stringa ci sia il nome di un oggetto
        codice = -1;
        // Cerca l'oggetto richiesto nella location
        for(; i < l->num_items; i++){
            if(strcmp(object, l->items[i].name) == 0){
                codice = 0;
                break;
            }
        }

        // Invia il codice di risposta
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0)
            return ret;

        if(codice < 0) // Se il codice è -1, l'oggetto richiesto non è presente nella location
            return 0;
        else{
            // Altrimenti è presente l'oggetto
            // Invia al Client il nome dell'oggetto
            ret = send_msg(sockfd, l->items[i].name, strlen(l->items[i].name), 0);
            if(ret < 0)
                return ret;
            
            // Invia al Client la descrizione dell'oggetto
            ret = send_msg(sockfd, l->items[i].description, strlen(l->items[i].description), 0);
            if(ret < 0)
                return ret;
            
            // Invia il codice 0 per indicare che la risposta è completa
            codice = 0;
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nell'invio del codice");
                return ret;
            }
        }
    }

    return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
}

// Comando che permette di interagiare o prendere un oggetto
int take(const int sockfd, const char* object, struct server_descriptor* s, const int index, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, i = 0, j = 0, codice = -1;
    struct location* l = &s[index].escape_room.rooms[p[giocatore].location];
    struct object* o;
    const char* descrizione1 = "La location "; // Prima parte della descrizione
    const char* descrizione2 = " non ha più oggetti, né nulla di interessante. "; // Terza parte della descrizione
    const char* aggiunta0 = "Ha interagito con l'oggetto "; // Nel caso ci sia prima un indovinello da risolvere
    const char* aggiunta1 = "Ha preso l'oggetto "; // Nel caso si prenda l'oggetto
    char* copia_nome; // Serve a copiare il nome dell'oggetto
    char* messaggio;

    // Verifica se l'oggetto è specificato
    if(object == NULL){
        codice = -1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice");
            return ret;
        }
        return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
    }

    if(l->points_needed > count_points(p)) // Verifica se il giocatore ha abbastanza punti per interagire con gli oggetti
        codice = -5;
    else{
        // Cerca l'oggetto nella location
        for(; i < l->num_items; i++){
            if(strcmp(l->items[i].name, object) == 0){
                o = &l->items[i];
                // Verifica se l'oggetto ha il suo indovinello risposto o non presente
                if(l->items[i].riddle_answer == 0){
                    // Gestione finale del gioco (se è l'oggetto per l'uscita)
                    if(l->is_final == 1 && o->type == 3){
                        codice = 200;
                        win = 1;
                        break;
                    }

                    // Verifica se il giocatore ha raggiunto il limite massimo di oggetti
                    if(p[giocatore].num_items == MAX_ITEMS){
                        codice = -2;
                        break;
                    }
                    
                    // Copia il nome dell'oggetto
                    copia_nome = (char*) malloc((strlen(o->name) + 1) * sizeof(char));
                    if(copia_nome == NULL){
                        codice = -1;
                        break;
                    }
                    strcpy(copia_nome, o->name);

                    // Aggiunge i punti bonus all'utente
                    if(l->items[i].bonus_points != 0)
                        p[giocatore].points += l->items[i].bonus_points;
                    
                    // Gestione oggetti utilizzabili e quindi possibili da mettere nell'inventario
                    if(l->items[i].usable == 1){
                        p[giocatore].items[p[giocatore].num_items] = *o;
                        p[giocatore].num_items++;
                    }
                    else
                        destroy_object(&l->items[i]);
                    
                    // Si toglie l'oggetto dalla location
                    for(j = i + 1; j < l->num_items; j++)
                        l->items[j-1] = l->items[j];
                    l->num_items--;
                    codice = 1;
                    
                    // Se la location non ha più oggetti, aggiorna la descrizione
                    if(l->num_items <= 0){
                        free(l->description);
                        l->description = (char*) malloc((strlen(descrizione1) + strlen(l->name) + strlen(descrizione2) + 1) * sizeof(char));
                        if(l->description == NULL){
                            free(copia_nome);
                            codice = -1;
                        }
                        else{
                            strcpy(l->description, descrizione1);
                            strcat(l->description, l->name);
                            strcat(l->description, descrizione2);
                        }   
                    }
                    else // Altrimenti chiama la rewrite_description per togliere l'oggetto dalla descrizione
                        rewrite_description(l, s[index].escape_room.type);
                }
                else if(l->items[i].player != -1) // Sull'oggetto ci sta interagendo un altro giocatore
                    codice = -3;
                else if(l->items[i].enigma.type == 2)
                    codice = -4; // L'oggetto ha un indovinello risolvibile utilizzando un altro oggetto
                else{
                    // L'oggetto è disponibile per l'interazione
                    codice = 0;
                    o->player = giocatore;
                }
                    
                break;
            }
        }
    }

    // Invia il codice di risposta al giocatore
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // Gestione del messaggio di sistema e invio dell'oggetto o della risposta al giocatore
    if(codice == 0){
        // Il giocatore sta lavorando con l'oggetto e quindi deve risolvere l'indovinello
        messaggio = (char*) malloc((strlen(aggiunta0) + strlen(o->name) + 1) * sizeof(char));
        if(messaggio != NULL){
            strcpy(messaggio, aggiunta0);
            strcat(messaggio, o->name);
            add_message(message_list, messaggio, p[giocatore].username, p[giocatore].location, giocatore, 0);
            free(messaggio);
        }
        ret = send_object(sockfd, o); // Vengono inviate le informazioni che riguardano l'oggetto
    }
    else if(codice == 1){
        // L'oggetto viene preso dal giocatore
        messaggio = (char*) malloc((strlen(aggiunta1) + strlen(copia_nome) + 1) * sizeof(char));
        if(messaggio != NULL){
            strcpy(messaggio, aggiunta1);
            strcat(messaggio, copia_nome);
            add_message(message_list, messaggio, p[giocatore].username, p[giocatore].location, giocatore, 0);
            free(messaggio);
        }
        ret = send_msg(sockfd, copia_nome, strlen(copia_nome), 0);
        free(copia_nome);

        if(ret < 0)
            return ret;

        return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
    }
    else if(codice == 200) // Sei uscito e quindi il gioco è finito
        return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
    else if(codice < 0){ 
        // Non si è preso l'oggetto
        if(codice == -3){
            // Se il codice è -3 si sa che ce l'ha un giocatore e viene mandato lo username di chi sta interagendo
            ret = send_msg(sockfd, p[o->player].username, strlen(p[o->player].username), 0);
            if(ret < 0)
                return ret;
        }
        return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
    }

    return ret; // Restituisce il risultato dell'operazione di spostamento
}

// Questa funzione continua il comando "take" se l'oggetto aveva un indovinello
int go_on(const int sockfd, struct server_descriptor* s, const int index, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, i = 0, risultato, codice;
    char* nome_oggetto;
    struct location* l = &s[index].escape_room.rooms[p[giocatore].location];

    // Riceve il risultato dell'indovinello dal Client
    ret = recv_all(sockfd, &risultato, sizeof(risultato), 0);
    if(ret < 0){
        perror("Errore nella ricezione del risultato \n");
        return ret;
    }

    // Riceve il nome dell'oggetto associato all'indovinello
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;

    // Alloca dinamicamente la memoria per il nome dell'oggetto e verifica se l'allocazione ha successo
    nome_oggetto = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(nome_oggetto == NULL){
        codice = -1;

        // Invia il codice di errore al Client
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice");
            return ret;
        }

        return codice;
    }
    strcpy(nome_oggetto, buffer);
    memset(buffer, 0, strlen(nome_oggetto) + 1);
    
    codice = -1; // Inizializza il codice di risposta a -1 (errore)
    // Cerca l'oggetto nella location
    for(; i < l->num_items; i++){
        if(strcmp(l->items[i].name, nome_oggetto) == 0){
            codice = 0;
            // Imposta il giocatore associato all'interazione con l'oggetto a -1
            l->items[i].player = -1;
            // Gestione dell'esito dell'indovinello
            if(risultato == 0){
                printf("Game Server %d: Fallimento dell'indovinello \n", index);
                // Se l'indovinello è a risposta multipla e si è fallito, la partita finisce
                if(l->items[i].enigma.type == 1){
                    l->items[i].enigma.attempts_left--;
                    if(l->items[i].enigma.attempts_left == 0){
                        timer_expired = 1;
                        reset_timer(timer, 0);
                        printf("Game Server %d: Partita Finita \n", index);
                    }
                }   
            }
            else if(risultato == 1){
                // Se l'indovinello ha successo, imposta la risposta dell'indovinello a 0
                l->items[i].riddle_answer = 0;
                printf("Game Server %d: Successo dell'indovinello \n", index);
            }
            break;
        }
    }

    // Invia il codice di risposta al Client
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0)
        return ret;

    // Se il codice è negativo, restituisci il codice di errore
    if(codice < 0)
        return codice;
    
    return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
}

// Comando che permette di usare un oggetto presente nell'inventario
int use(const int sockfd, const char* object, struct server_descriptor* s, const int index, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, i = 0, j = 0, k = 0, codice = 0, tipo, tempo_bonus = 0;
    struct location* l = &s[index].escape_room.rooms[p[giocatore].location];
    struct itimerval remaining_time;

    // Verifica se l'oggetto è nullo. object è il nome dell'oggetto nell'inventario.
    if(object == NULL)
        codice = -1;
    
    // Invia 0 se la stringa object non è nulla, invia -1 se invece è nulla
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // Se la stringa è nulla esce
    if(codice < 0)
        return codice;

    codice = -1; // Inizializza il codice di risposta a -1 (errore)

    // Ciclo per cercare l'oggetto nell'inventario del giocatore
    for(; i < p[giocatore].num_items && i < MAX_ITEMS; i++){
        if(strcmp(p[giocatore].items[i].name, object) == 0){
            // Verifica se l'oggetto è utilizzabile
            if(p[giocatore].items[i].usable == 1){
                tipo = p[giocatore].items[i].type;

                if(p[giocatore].items[i].opens_door == 1){
                    codice = 1;
                    for(; j < l->num_doors; j++){
                        // Se l'oggetto apre una porta, gestisci la logica di apertura
                        if(l->linked_locations[j].next_location == p[giocatore].items[i].location_index && l->linked_locations[j].is_blocked == 1){
                            l->linked_locations[j].is_blocked = 0;
                            // Apre anche la porta della location che riporta indietro
                            for(; k < s[index].escape_room.rooms[l->linked_locations[j].next_location].num_doors; k++){
                                if(s[index].escape_room.rooms[l->linked_locations[j].next_location].linked_locations[k].next_location == p[giocatore].location && s[index].escape_room.rooms[l->linked_locations[j].next_location].linked_locations[k].is_blocked == 1){
                                    s[index].escape_room.rooms[l->linked_locations[j].next_location].linked_locations[k].is_blocked = 0;
                                    break;
                                }
                            }
                            codice = 0;
                            break;
                        }
                    }

                    // Se usato rimuove l'oggetto dall'inventario del giocatore
                    if(codice == 0){
                        destroy_object(&p[giocatore].items[i]);
                        for(j = i + 1; j < p[giocatore].num_items && j < MAX_ITEMS; j++)
                            p[giocatore].items[j-1] = p[giocatore].items[j];
                        p[giocatore].num_items--;
                    }
                    break;
                }     

                // Se l'oggetto fornisce un bonus di tempo, gestisci la logica del bonus di tempo
                if(p[giocatore].items[i].bonus_time != 0){
                    // Ottieni il tempo rimanente dal timer e gestisci in caso di errore
                    if(getitimer(ITIMER_REAL, &remaining_time) == -1){
                        perror("Errore nell'ottenere il tempo rimanente del timer");
                        ret = send_all(sockfd, &codice, sizeof(codice), 0);
                        if(ret < 0){
                            perror("Errore nell'invio del codice");
                            return ret;
                        }
                        return codice;
                    }
                    
                    // Aggiungi il bonus di tempo al tempo rimanente
                    tempo_bonus = p[giocatore].items[i].bonus_time;
                    remaining_time.it_value.tv_sec += p[giocatore].items[i].bonus_time;
                    reset_timer(timer, remaining_time.it_value.tv_sec);
                    
                    // Rimuovi l'oggetto dall'inventario del giocatore
                    destroy_object(&p[giocatore].items[i]);
                    for(j = i + 1; j < p[giocatore].num_items && j < MAX_ITEMS; j++)
                        p[giocatore].items[j-1] = p[giocatore].items[j];
                    p[giocatore].num_items--;
                    codice = 0;
                    break;
                }          
            }
            // Se l'oggetto è utilizzabile, imposta il codice di risposta a 1
            codice = 1;        
            break;            
        }
    }

    // Invia il codice di risposta al Client
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // Se il codice è 0, invia il tipo dell'oggetto e il tempo bonus
    if(codice == 0){
        ret = send_all(sockfd, &tipo, sizeof(tipo), 0);
        if(ret < 0){
            perror("Errore nell'invio del tipo dell'oggetto");
            return ret;
        }

        ret = send_all(sockfd, &tempo_bonus, sizeof(tempo_bonus), 0);
        if(ret < 0){
            perror("Errore nell'invio del tipo dell'oggetto");
            return ret;
        }
    }

    return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
}

// Comando "use" che permette di risolvere un indovinello utilizzando un oggetto
int use_object(const int sockfd, const char* object1, const char* object2, struct server_descriptor* s, const int index, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, i = 0, j = 0, codice = 1, indice;
    struct location* l = &s[index].escape_room.rooms[p[giocatore].location];
    char* supporto;
    const char* aggiunta = " Tuttavia... adesso è presente l'oggetto: **";
    const char* fine = "**.";

    // Verifica se uno delle due stringhe object è nulla. object1 è il nome dell'oggetto nell'inventario, object2 è il nome dell'oggetto nella location.
    if(object1 == NULL || object2 == NULL)
        codice = -1;
    
    // Invia il codice al Client
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // Se il codice è negativo, restituisci il codice di errore
    if(codice < 0)
        return codice;

    // Inizializza il codice di risposta a -1 (errore)
    codice = -1; 
    
    // Cerca l'object1 nell'inventario del giocatore
    for(; i < p[giocatore].num_items && i < MAX_ITEMS; i++){
        if(strcmp(p[giocatore].items[i].name, object1) == 0){
            // Verifica se l'object1 è utilizzabile e non fornisce bonus di tempo
            if(p[giocatore].items[i].usable == 1 && p[giocatore].items[i].bonus_time == 0){
                codice = 0;
                indice = i; // Teniamo a mente l'indice nell'inventario di object1
            }
            else
                codice = 1;            
            break;            
        }
    }

    // Invia il codice di risposta al Client. Se è 0 l'oggetto è presente, se è 1 è presente ma non si può usare, se è -1 c'è un errore.
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // Se il codice è 0, procedi con l'utilizzo dell'object2. 
    if(codice == 0){
        codice = -1;
        for(i = 0; i < l->num_items; i++){
            if(strcmp(l->items[i].name, object2) == 0){
                // Verifica se l'object2 ha un indovinello di tipo 2 e la soluzione è l'object1
                if(l->items[i].enigma.type == 2 && strcmp(l->items[i].enigma.solution, p[giocatore].items[indice].name) == 0){
                    // Rimuovi l'object1 dall'inventario del giocatore grazie all'indice memorizzato prima
                    destroy_object(&p[giocatore].items[indice]);
                    for(j = indice + 1; j < p[giocatore].num_items && j < MAX_ITEMS; j++)
                        p[giocatore].items[j-1] = p[giocatore].items[j];
                    p[giocatore].num_items--;
                    // Viene aggiornato il nome dell'oggetto
                    if(l->items[i].name != NULL)
                        free(l->items[i].name);
                    l->items[i].name = (char*) malloc((strlen(l->items[i].enigma.question) + 1) * sizeof(char));
                    if(l->items[i].name == NULL){
                        codice = -1;
                        break;
                    }
                    strcpy(l->items[i].name, l->items[i].enigma.question); // Viene usata il nome memorizzato in question
                    codice = 0;
                    // indice = i; // indice è aggiornato al valore di i
                    l->items[i].riddle_answer = 0;

                    // Aggiorna la descrizione della stanza se necessario, perché l'oggetto cambia nome
                    if(l->items[i].type != 3){
                        supporto = (char*) malloc((strlen(l->description) + 1) * sizeof(char));
                        if(supporto == NULL){
                            printf("Errore nell'allocazione della stringa di supporto \n");
                            break;
                        }
                        strcpy(supporto, l->description);
                        free(l->description);

                        l->description = (char*) malloc((strlen(supporto) + strlen(aggiunta) + strlen(l->items[i].name) + strlen(fine) + 1) * sizeof(char));
                        if(l->description == NULL){
                            printf("Errore nell'allocazione della descrizione della stanza \n");
                            codice = -1;
                            break;
                        }

                        strcpy(l->description, supporto);
                        strcat(l->description, aggiunta);
                        strcat(l->description, l->items[i].name);
                        strcat(l->description, fine);
                        free(supporto);
                    }
                    
                }
                else
                    codice = 1;
                break;
            }
        }

        // Invia il codice di risposta al Client
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice");
            return ret;
        }

        // Se il codice è 0, invia il nome dell'object2 aggiornato
        if(codice == 0){
            ret = send_msg(sockfd, l->items[i].name, strlen(l->items[i].name), 0);
            if(ret < 0)
                return ret;
        }
    }

    return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
}

// Comando per vedere gli oggetti che si hanno nell'inventario
int objs(const int sockfd, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, i = 0;

    // Invia il numero di oggetti nell'inventario del giocatore al Client
    ret = send_all(sockfd, &p[giocatore].num_items, sizeof(p[giocatore].num_items), 0);
    if(ret < 0){
        perror("Errore nell'invio del numero di oggetti");
        return ret;
    }

    // Se il giocatore non ha oggetti, invia solo il tempo rimanente e altri dettagli al giocatore
    if(p[giocatore].num_items <= 0)
        return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore

    // Invia i nomi degli oggetti nell'inventario al client
    for(; i < p[giocatore].num_items; i++){
        ret = send_msg(sockfd, p[giocatore].items[i].name, strlen(p[giocatore].items[i].name), 0);
        if(ret < 0)
            return ret;
    }

    return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
}

// Comando per vedere a quali location si può accedere
int doors(const int sockfd, struct server_descriptor* s, const int index, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, i = 0;
    struct escape_room_descriptor* r = &s[index].escape_room;
    struct location* l = &s[index].escape_room.rooms[p[giocatore].location];

    // Invia il numero di porte della locazione corrente al Client
    ret = send_all(sockfd, &l->num_doors, sizeof(l->num_doors), 0);
    if(ret < 0){
        perror("Errore nell'invio del numero di oggetti");
        return ret;
    }

    // Se la locazione non ha porte, invia solo il tempo rimanente e altri dettagli al giocatore
    if(l->num_doors <= 0)
        return send_time(sockfd, p, giocatore, message_list, timer);

    // Invia i nomi delle locazioni raggiungibili e le informazioni sulla loro accessibilità al Client
    for(; i < l->num_doors; i++){
        ret = send_msg(sockfd, r->rooms[l->linked_locations[i].next_location].name, strlen(r->rooms[l->linked_locations[i].next_location].name), 0);
        if(ret < 0)
            return ret;

        ret = send_all(sockfd, &l->linked_locations[i].is_blocked, sizeof(l->linked_locations[i].is_blocked), 0);
        if(ret < 0){
            perror("Errore nell'invio delle informazioni riguardo all'accessibilità delle locazioni");
            return ret;
        }
    }

    return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
}

// Comando che permette di inviare messaggi agli altri giocatori
int message(const int sockfd, struct message** message_list, const char* phrase, struct server_descriptor* s, const int index, struct player_descriptor* p, const int giocatore, struct itimerval* timer){
    int ret, codice = 0;

    // Aggiunge il messaggio alla lista dei messaggi, come messaggio NON di sistema, ma mandato da un giocatore
    add_message(message_list, phrase, p[giocatore].username, p[giocatore].location, giocatore, 1);
    
    // Verifica se l'aggiunta del messaggio è avvenuta correttamente
    if(message_list == NULL)
        codice = -1;

    // Invia il codice risultante al client
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    return send_time(sockfd, p, giocatore, message_list, timer); // Invia il tempo rimanente e altri dettagli al giocatore
}

// Finisce la partita
int end(const int sockfd, struct server_descriptor* s, const int index, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, codice = 0;
    const char* messaggio = "Ha digitato end. La partita non può più continuare e quindi è finita!";

    // Aggiunge il messaggio di fine partita alla lista dei messaggi come messaggio di sistema
    add_message(message_list, messaggio, p[giocatore].username, -1, giocatore, 0);    

    // Invia il codice risultante al Client
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // La partita finisce
    timer_expired = 1;
    reset_timer(timer, 0);   

    return 0;
}

// Timeout inviato dal Client per ricevere le informazioni sullo stato della partita
int timeout(const int sockfd, struct message** message_list, struct player_descriptor* p, const int giocatore){
    int ret, codice = 0;
    
    // Se la partita è già stata vinta o il timer è scaduto, invia il codice 200 al Client che indica che la partita è conclusa in ogni caso
    if(win > 0 || timer_expired > 0){
        codice = 200;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice di stato");
            return ret;
        }
    }
    // Altrimenti se ci sono messaggi pendenti per il giocatore, invia il codice 1 e gestisce l'invio dei messaggi
    else if(count_pending_messages(*message_list, giocatore) > 0){
        codice = 1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice di stato");
            return ret;
        }

        ret = control_messages(sockfd, message_list, giocatore, p);
        if(ret < 0)
            return 0;
    }
    // Se non ci sono messaggi pendenti e la partita non è finita, invia il codice 0 e continua
    else{
        codice = 0;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice di stato");
            return ret;
        }
    }

    return 0;
}

// Invia il tempo rimanente, le informazioni sui giocatori ed eventuali messaggi
int send_time(const int sockfd, struct player_descriptor* p, const int giocatore, struct message** message_list, struct itimerval* timer){
    int ret, i = 0, minuti, secondi, giocatori = 0, codice = 0;
    struct itimerval remaining_time;

    // Ottieni il tempo rimanente dal timer
    if(getitimer(ITIMER_REAL, &remaining_time) == -1){
        perror("Errore nell'ottenere il tempo rimanente del timer");
        return -1;
    }

    // Calcola i minuti e i secondi rimanenti
    minuti = remaining_time.it_value.tv_sec / 60;
    secondi = remaining_time.it_value.tv_sec % 60;

    // Ci si assicura che minuti e secondi non siano negativi

    if(minuti < 0)
        minuti = 0;

    if(secondi < 0)
        secondi = 0;

    // Invia i minuti al Client
    ret = send_all(sockfd, &minuti, sizeof(minuti), 0);
    if(ret < 0){
        perror("Errore nell'invio dei minuti");
        return ret;
    }

    // Invia i secondi al Client
    ret = send_all(sockfd, &secondi, sizeof(secondi), 0);
    if(ret < 0){
        perror("Errore nell'invio dei secondi");
        return ret;
    }

    // Conta il numero totale di giocatori attivi 
    for(; i < MAX_CLIENTS; i++){
        if(p[i].is_set == 1)
            giocatori++;
    }
    
    // Invia il numero di giocatori al Client
    ret = send_all(sockfd, &giocatori, sizeof(giocatori), 0);
    if(ret < 0){
        perror("Errore nell'invio del numero dei giocatori");
        return ret;
    }
    
    // Invia le informazioni sui giocatore attivi: username e punteggio
    for(i = 0; i < MAX_CLIENTS; i++){
        if(p[i].is_set == 1){            
            ret = send_msg(sockfd, p[i].username, strlen(p[i].username), 0);
            if(ret < 0)
                return ret;
            
            ret = send_all(sockfd, &p[i].points, sizeof(p[i].points), 0);
            if(ret < 0){
                perror("Errore nell'invio del punteggio");
                return ret;
            }
        }   
    }

    if(count_pending_messages(*message_list, giocatore) > 0){
        // Se ci sono messaggi pendenti per il giocatore, invia il codice 1 e gestisci i messaggi
        codice = 1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice di stato");
            return ret;
        }

        // Controlla e invia i messaggi pendenti al giocatore
        ret = control_messages(sockfd, message_list, giocatore, p);
        if(ret < 0)
            return 0;
    }
    else{
        // Se non ci sono messaggi pendenti, invia il codice 0
        codice = 0;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice di stato");
            return ret;
        }
    }

    return 0;
}

// FINE PROGRAMMA