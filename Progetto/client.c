// Progetto: ESCAPE ROOM
// Autore: Francesco Panattoni
// Matricola: 604230

// Il main si trova alla riga 210

// LIBRERIE
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>

// COSTANTI
#define BUFFER_SIZE 1024
#define TURN 3 // Ogni quanto si invia il timeout durante il gioco. Non deve essere <= 0!
#define RIDDLE_TIME 120 // Quanto dura il timeout per l'indovinello
// Definizione di sequenze di escape ANSI per i colori
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"


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


// VARIABILI GLOBALI
char buffer[BUFFER_SIZE];
const int KEY = 0xFADA; // Chiave usata per criptare la password
char *user;


// DICHIARAZIONE DELLE FUNZIONI
int start_main_connection(const int);
void close_connection(const int);
int signup(const int);
int login(const int);
int logout(const int);
char* read_password();
void encrypt(char*);
int start_game_connection(const int);
int recv_rooms_informations(const int);
int game_escape_room(const int, const int);
void get_input_with_timeout();
void get_input_for_riddle();
int recv_object(const int, struct object*);
int control_messages(const int);

// Comandi
int handle_commands(const int, char*, const int, int*);
int begin(const int);
int quit(const int);
int go_to(const int, char*, const int);
int look(const int, const char*, const int);
int take(const int, const char*, const int, int*);
int go_on(const int, struct object*, const int, int*);
int use(const int, const char*, const int);
int objs(const int, const char*, const int);
int doors(const int, const char*, const int);
int message(const int, const char*, const int);
int help();
int end(const int, const char*, const int);
int recv_time(const int, const int);
void print_commands();
int timeout(const int, const char*, const int);


// FUNZIONI UTILI 

// Custom send che si impegna ad inviare tutti i dati
int send_all(const int sockfd, const void* buf, size_t size, int flags){
    size_t total_sent = 0; 
    ssize_t sent_bytes;     

    while(total_sent < size){
        sent_bytes = send(sockfd, (char*)buf + total_sent, size - total_sent, flags);
        if(sent_bytes <= 0){
            perror("Errore durante l'invio. \n");
            if(sent_bytes == 0)
                sent_bytes = -1;
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

    if(total_received == 0)
        return -1;

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
        perror("Errore nell'invio della lunghezza del messaggio");
        return ret;
    }

    strcpy(buffer, msg);
    ret = send_all(sockfd, buffer, length, flags);
    if(ret < 0){
        perror("Errore nell'invio del messaggio");
        return ret;
    }

    return 1;
}

// Custom recv che riceve un messaggio
int recv_msg(const int sockfd, char* buf, int flags){
    int ret, length;

    ret = recv_all(sockfd, &length, sizeof(length), flags);
    if(ret < 0){
        perror("Errore nella ricezione della lunghezza del messaggio");
        return ret;
    }

    if(length <= 0){
        printf("Errore nella ricezione della lunghezza del messaggio. \n");
        return -1;
    }

    memset(buf, 0, length+1);

    ret = recv_all(sockfd, buf, length, flags);
    if(ret < 0){
        perror("Errore nella ricezione del messaggio");
        return ret;
    }

    buf[length] = '\0';
    return length;
}


// INIZIO PROGRAMMA
int main(int argc, char* argv[]){
    int port, connection;

    // Verifica se il numero corretto di argomenti è stato fornito
    if(argc != 2) {
        printf("Errore! Per attivare l'applicazione c'è bisogno che tu inserisca: \n");
        printf("./%s <port> \n", argv[0]);
        printf("<port> deve essere un numero valido per una porta! \n");
        exit(EXIT_FAILURE);
    }

    // Estrai il numero di porta dalla riga di comando, che indica la porta del Main Server
    port = atoi(argv[1]);

    // Inizializza il buffer e stampa un messaggio di apertura del Client
    memset(buffer, 0, sizeof(buffer));
    // Avvia la connessione al Main Server
    connection = start_main_connection(port);
    if(connection != -1) // Chiusura della connessione
        close_connection(connection);
    else
        printf("Errore nell'inserimento della porta! \n");
    return 0;
}


int start_main_connection(const int port){
    int sd, ret, codice;
    struct sockaddr_in server_address;
    const char* exit_msg = "exit";

    // Creazione Socket per la connessione al Main Server
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if(sd < 0){
        perror("Errore nella creazione del Socket");
        exit(EXIT_FAILURE);
    }

    // Creazione Indirizzo del Socket
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET; 
    server_address.sin_port = htons(port); 
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr); 

    // Creo la connessione al Main Server
    ret = connect(sd, (struct sockaddr*)& server_address, sizeof(server_address));
    if(ret < 0){
        perror("Errore nella creazione della Connessione");
        exit(EXIT_FAILURE);
    }

    printf("Connessione al Main Server... \nTi diamo il benvenuto al gioco ESCAPE ROOM! \n");
    
    while(1){
        printf(ANSI_COLOR_RED "Scegliere una opzione tra signup, login e exit. \n1) signup --> registri un nuovo account \n2) login --> entri con un account già registrato \n3) exit --> chiudi la connessione col server");
        printf(ANSI_COLOR_RESET "\n");

        fflush(stdin);
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        
        // Invio della richiesta
        if(strncmp(buffer, "signup", 6) == 0 || strncmp(buffer, "Signup", 6) == 0 || strncmp(buffer, "SIGNUP", 6) == 0){
            // Richiesta di signup
            ret = signup(sd);
            if(ret < 0)
                printf("La registrazione non è andata a buon termine.\n");
            else
                break;
        }
        else if(strncmp(buffer, "login", 5) == 0 || strncmp(buffer, "Login", 5) == 0 || strncmp(buffer, "LOGIN", 5) == 0){
            // Richiesta di login
            ret = login(sd);
            if(ret < 0)
                printf("Il login non è andato a buon fine.\n");
            else
                break;
        }
        else if(strncmp(buffer, "exit", 4) == 0 || strncmp(buffer, "Exit", 4) == 0 || strncmp(buffer, "EXIT", 4) == 0){
            // Richiesta di chiusura della connessione
            codice = 0;
            ret = send_all(sd, &codice, sizeof(codice),0);
            if(ret < 0){
                perror("Errore nell'invio del codice");
                return sd; // Ritorno il socket da chiudere
            }

            // Invio del messaggio di uscita al Server
            ret = send_msg(sd, exit_msg, strlen(exit_msg), 0);
            if(ret < 0)
                return sd; // Ritorno il socket da chiudere
            return sd; // Ritorno il socket da chiudere
        }
        else
            printf("Non hai digitato un comando valido. Riprova. \nScegliere una opzione tra signup, login e exit. \n1) signup --> registri un nuovo account \n2) login --> entri con un account già registrato \n3) exit --> chiudi la connessione col server\n");
    }

    printf("In attesa della creazione di un Game Server \n");
    
    memset(buffer, 0, sizeof(buffer));
    do{ 
         // Notifica il Server della volontà di volersi collegare ad un Game Server
        codice = 1;
        ret = send_all(sd, &codice, sizeof(codice), 0);
        if(ret < 0){
            free(user);
            return sd; // Ritorno il socket da chiudere
        }
        
        // Ricezione delle informazioni sulle stanze dal Server
        ret = recv_rooms_informations(sd); // Se ci sono delle porte attive restituisce 0
        if(ret == 0)
            ret = start_game_connection(sd); // Stabilisce la connessione col Game Server
    } while(ret != 0);

    // Logout e deallocazione delle risorse
    ret = logout(sd);
    // Deallocazione delle risorse e ritorno il socket da chiudere
    free(user);
    return sd; 
}


void close_connection(const int connection){
    printf("Chiudo la connessione al Server \n");
    close(connection);
}

// Funzione di registrazione al Main Server
int signup(const int sockfd){
    int ret = 0, lunghezza = 0, risultato, codice = 0;
    char* username;
    char* password;
    char* confirm_password;
    const char* signup = "signup";

    // Acquisizione dell'username
    while(1){
        memset(buffer, 0, sizeof(buffer));

        printf("Inserisci l'username:   ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';
        lunghezza = strlen(buffer);
        if(lunghezza < 2 || lunghezza > 16)
            continue;
        
        // Alloca dinamicamente la memoria per l'username e verifica se l'allocazione ha successo
        username = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
        if(username == NULL){
            printf("Errore nell'inserimento dello username!\n");
            return -1;
        }
        strcpy(username, buffer);
        memset(buffer, 0, strlen(username) + 1);
        break;
    }

    // Acquisizione e conferma della password
    while(1){
        printf("Inserisci la password:  ");
        password = read_password(); // Rende la password illeggibile a schermo
        lunghezza = strlen(password);
        if(password == NULL){
            printf("Errore nell'inserimento della password!\n");
            free(username);
            return -1;
        }
        if(lunghezza < 2 || lunghezza > 16)
            continue;
        

        printf("Conferma la password:  ");
        confirm_password = read_password(); // Rende la password illeggibile a schermo
        lunghezza = strlen(confirm_password);
        if(confirm_password == NULL){
            printf("Errore nell'inserimento della password!\n");
            free(username);
            free(password);
            return -1;
        }
        if(lunghezza < 2 || lunghezza > 16){
            free(password);
            continue;
        }   

        // Confronto tra la password e la sua conferma
        if(strcmp(password, confirm_password) == 0)
            break;
        else{
            printf("Password diverse. Reinserisci! \n");
            free(confirm_password);
            free(password);
        }
    }

    free(confirm_password);
    encrypt(password); // Crittografia della password

    // Invio del codice al Main Server
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice della registrazione");
        free(username);
        free(password);
        return ret;
    }

    // Invio del comando di registrazione al Main Server
    ret = send_msg(sockfd, signup, strlen(signup), 0);
    if(ret < 0){
        free(username);
        free(password);
        return ret;
    }

    // Invio dell'username al Main Server
    ret = send_msg(sockfd, username, strlen(username), 0);
    if(ret < 0){
        free(username);
        free(password);
        return ret;
    }
    
    // Invio della password crittografata al Main Server
    ret = send_msg(sockfd, password, strlen(password), 0);
    if(ret < 0){
        free(username);
        free(password);
        return ret;
    }

    free(password);
    
    // Ricezione del risultato della registrazione dal server
    ret = recv_all(sockfd, &risultato, sizeof(risultato), 0);
    if(ret < 0){
        perror("Errore nella ricezione del risultato della registrazione");
        free(username);
        return ret;
    }

    // Elaborazione del risultato della registrazione
    if(risultato == 0){
        // Registrazione avvenuta con successo
        // Alloca dinamicamente la memoria per l'username e verifica se l'allocazione ha successo
        user = (char*) malloc((strlen(username) + 1) * sizeof(char));
        if(user == NULL){
            free(username);
            free(password);
            return -1;
        }
        strcpy(user, username); // user è una stringa globale che contiene lo username preso col signup o col login
        printf("Main Server: Registrazione avvenuta con successo \n");
    }
    else if(risultato == 1){
        // Nome utente già esistente
        printf("Main Server: Registrazione non avvenuta. Questo nome utente esiste già! \n");
        risultato = -1;
    }
    
    return risultato;
}

// Funzione di login al Main Server
int login(const int sockfd){
    int ret = 0, risultato, codice = 0;
    char* username;
    char* password;
    const char* login = "Login";

    // Acquisizione dell'username
    printf("Inserisci l'username:   ");
    fgets(buffer, sizeof(buffer), stdin);
    buffer[strcspn(buffer, "\n")] = '\0';

    // Alloca dinamicamente la memoria per l'username e verifica se l'allocazione ha successo
    username = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(username == NULL){
        printf("Errore nell'inserimento dello username!\n");
        return -1;
    }
    strcpy(username, buffer);
    memset(buffer, 0, strlen(username) + 1);

    // Acquisizione e crittografia della password
    printf("Inserisci la password:  ");
    password = read_password();
    if(password == NULL){
        printf("Errore nell'inserimento della password!\n");
        free(username);
        return -1;
    }
    encrypt(password); // Crittografia della password

    // Invio del codice al Main Server
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice iniziale");
        return ret;
    }

    // Invio del comando di login al Main Server
    ret = send_msg(sockfd, login, strlen(login), 0);
    if(ret < 0){
        free(username);
        free(password);
        return ret;
    }   

    // Invio dell'username al Main Server
    ret = send_msg(sockfd, username, strlen(username), 0);
    if(ret < 0){
        free(username);
        free(password);
        return ret;
    }
    
    // Invio della password crittografata al Main Server
    ret = send_msg(sockfd, password, strlen(password), 0);
    if(ret < 0){
        free(username);
        free(password);
        return ret;
    }

    // Ricezione del risultato del login dal Main Server
    ret = recv_all(sockfd, &risultato, sizeof(risultato), 0);
    if(ret < 0){
        perror("Errore nella ricezione del risultato del login");
        free(username);
        free(password);
        return ret;
    }

    // Elaborazione del risultato del login
    if(risultato == 0){
        // Login avvenuto con successo
        // Alloca dinamicamente la memoria per l'username e verifica se l'allocazione ha successo
        user = (char*) malloc((strlen(username) + 1) * sizeof(char));
        if(user == NULL){
            free(username);
            free(password);
            return -1;
        }
        strcpy(user, username); // user è una stringa globale che contiene lo username preso col login o col signup
        printf("Main Server: Login avvenuto con successo ...\n");
    }
    if(risultato == 1){
        // Utente già loggato
        printf("Main Server: Questo username ha già fatto l'accesso \n");
        risultato = -1;
    }

    // Liberazione della memoria utilizzata per username e password
    free(username);
    free(password);

    return risultato;
}

// Funzione di logout dal Main Server
int logout(const int sockfd){
    int ret, codice = 3;

    // Invio del codice di logout
    ret = send_all(sockfd, &codice, sizeof(codice), 0);    
    if(ret < 0)
        return ret;

    // Invio dello username
    ret = send_msg(sockfd, user, strlen(user), 0);
    if(ret < 0)
        return ret;

    return 0;
}

// Funzione per leggere la password, senza far vedere i caratteri inseriti a schermo
char* read_password(){
    int indice = 0;
    int dimensione = 32; // Dimensione iniziale del buffer della password
    struct termios oldt, newt; // Vengono utilizzate per memorizzare e manipolare le impostazioni del terminale
    char ch;
    char *password = (char*) malloc(dimensione * sizeof(char)); // Allocazione iniziale della memoria

    if(password == NULL) {
        printf("Errore di allocazione della memoria.\n");
        return NULL;
    }    

    // Ottenere le impostazioni attuali del terminale
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt; // Copio le impostazioni
    newt.c_lflag &= ~(ECHO); // Nascondo l'input
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Lettura dell'input carattere per carattere
    while(1){
        ch = getchar();

        if(ch == '\n' || ch == '\r'){
            break; // Se viene premuto Invio, esci dal ciclo
        }
        else if (ch == 127 && indice > 0){
            printf("\b \b"); // Cancella un carattere visualmente
            indice--;
        }
        else{
            password[indice] = ch; // Memorizza il carattere nella stringa della password

            // Se il buffer è pieno, rialloca il doppio della dimensione attuale
            if (indice == BUFFER_SIZE - 1) {
                dimensione*= 2;
                password = (char*) realloc(password, dimensione * sizeof(char));
                if (password == NULL) {
                    printf("Errore di riallocazione della memoria.\n");
                    return NULL;
                }
            }

            putchar('*'); // Stampa un asterisco (o qualsiasi carattere desideri per nascondere l'input)
            indice++;
        }
    }

    password[indice] = '\0'; // Aggiungi il terminatore di stringa alla password
    printf("\n");

    // Ripristina le impostazioni originali del terminale
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return password; // Restituisce il puntatore alla stringa password
}

// Semplice funzione che cripta la password usando una versione modificata del cifratio di Cesare
void encrypt(char* password) {
    int i = 0;
    char c;

    // I %26 o %10 sono stati messi per non generare caratteri speciali che non venissero letti dal file in fase di login 

    for(; i < strlen(password); i++){
        c = password[i];

        if('a' <= c && c <= 'z')
            password[i] = 'a' + (c - 'a' + KEY) % 26;
        else if('A' <= c && c <= 'Z')
            password[i] = 'A' + (c - 'A' + KEY) % 26;
        else if('0' <= c && c <= '9')
            password[i] = '0' + (c - '0' + KEY) % 10;
    }
}

// Inizia e gestisce la connessione con un Game Server
int start_game_connection(const int sockfd){
    int nuova_porta, ret, codice = 2, stato, newsd, newcon;
    struct sockaddr_in server_address;

    // L'utente sceglie la porta del Game Server
    printf("Scegli la porta da inviare: ");
    do{
        memset(buffer, 0, sizeof(buffer));
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Rimuove il carattere di nuova riga dalla stringa   
    } while(buffer[0] == '\0');

    nuova_porta = atoi(buffer);
    fflush(stdin);

    // Invia il codice 2 al Main Server per avviare la connessione con un Game Server
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // Invia lo username al Main Server
    ret = send_msg(sockfd, user, strlen(user), 0);
    if(ret < 0)
        return ret;

    // Riceve un codice dal Main Server per indicare se ha allocato senza errori lo username
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }

    // Se il codice è negativo restituisce un errore
    if(codice < 0)
        return codice;

    // Invia la porta del Game Server al Main Server
    ret = send_all(sockfd, &nuova_porta, sizeof(nuova_porta), 0);
    if(ret < 0){
        perror("Errore nell'invio della porta");
        return ret;
    }

    // Riceve un codice dal Main Server
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'a ricezione del codice");
        return ret;
    }

    // Se il codice è negativo restituisce un errore
    if(codice < 0)
        return codice;

    // Creazione Socket per la connessione al Game Server
    newsd = socket(AF_INET, SOCK_STREAM, 0);
    if(newsd < 0){
        perror("Errore nella creazione del Socket");
        exit(EXIT_FAILURE);
    }

    // Creazione Indirizzo del Socket
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET; 
    server_address.sin_port = htons(nuova_porta); 
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr); 

    // Creo la connessione con il Game Server
    newcon = connect(newsd, (struct sockaddr*)& server_address, sizeof(server_address));
    if(newcon < 0){
        perror("Errore nella creazione della Connessione");
        exit(EXIT_FAILURE);
    }

    // Riceve un codice dal Game Server che è lo stato della partita
    ret = recv_all(newsd, &stato, sizeof(stato), 0);
    if(ret < 0){
        perror("Errore nella creazione della Connessione");
        close(newsd);
        return ret;
    }

    // Riceve il numero del Game Server
    ret = recv_all(newsd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella creazione della Connessione");
        close(newsd);
        return ret;
    }
            
    
    // Se il codice è 0 significa che non è ancora iniziata la partita e pertanto ci si può connettere
    if(stato != 0){             
        printf("Il Game Server %d è già in gioco, sceglierne un altro \n", codice);
        close(newsd);
        return 1; // Per non far terminare anche la connessione al Game Server
    }

    printf("Connessione al Game Server %d \n", codice);

    // Invia il nome utente al Game Server
    ret = send_msg(newsd, user, strlen(user), 0);
    if(ret < 0){
        close(newsd);
        return ret;
    }

    fflush(stdin);
    fflush(stdout);
    memset(buffer, 0, sizeof(buffer));

    return game_escape_room(newsd, codice); // Comincia il gioco dell'Escape Room
}

// Questa funzione riceve le informazioni riguardo alle Escape Room implementate dai Game Server
int recv_rooms_informations(const int sockfd){
    int ret, i = 0, servers, porta, codice;

    // Riceve il numero di Escape Room implementate dal Game Server
    ret = recv_all(sockfd, &servers, sizeof(servers), 0);    
    if(ret < 0){
        perror("Errore nella ricezione delle informazioni riguardanti le Escape Room");
        return ret;
    }

    // Se non ci sono Escape Room, restituisce un valore negativo
    if(servers <= 0)
        return -1;

    // Invia il nome utente al Game Server
    ret = send_msg(sockfd, user, strlen(user), 0);
    if(ret < 0)
        return ret;

    // Riceve un codice dal Game Server
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);    
    if(ret < 0){
        perror("Errore nella ricezione delle informazioni riguardanti le Escape Room");
        return ret;
    }

    // Se il codice è negativo, lo restituisce
    if(codice < 0)
        return codice;

    printf("\n");

    // Itera attraverso le Escape Room ricevendo e stampando le informazioni
    for(; i < servers; i++){
        // Riceve la porta della Escape Room
        ret = recv_all(sockfd, &porta, sizeof(porta), 0);    
        if(ret < 0){
            perror("Errore nella ricezione delle informazioni riguardanti le Escape Room");
            return ret;
        }

        // Riceve il tema della Escape Room
        ret = recv_msg(sockfd, buffer, 0);
        if(ret < 0)
            return ret;

        printf(ANSI_COLOR_YELLOW "Room %d: %s \n", porta, buffer);

        // Riceve la descrizione della Escape Room
        ret = recv_msg(sockfd, buffer, 0);
        if(ret < 0)
            return ret;

        printf("%s \n", buffer);
        printf(ANSI_COLOR_RESET "\n");
    }

    return 0;
}

// Questa funzione implementa il gioco dell'Escape Room dialogando col Game Server
int game_escape_room(const int sockfd, const int game_server){
    int in_game = 0; // Se è 0 indica che la partita non è iniziata, quando è 1 indica che la partita è iniziata
    int starting = 0; // Flag che indica se il turno di inizio è già avvenuto (0 no, 1 sì)
    int ret, codice;

    char* comando; // Comando da inviare
    
    while(1){
        fflush(stdout);
        if(in_game == 0){
            fflush(stdin);
            // Se la partita non è iniziata, l'utente può digitare "begin" per avviare il gioco o "quit" per uscire dal Game Server
            printf(ANSI_COLOR_RED "Digita un comando: \n");
            printf("1) begin--> avvia il gioco \n");
            printf("2) quit --> esce dal Game Server \n");    
            printf(ANSI_COLOR_RESET "\n");

            do{
                memset(buffer, 0, sizeof(buffer));
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = '\0'; // Rimuove il carattere di nuova riga dalla stringa   
            } while(buffer[0] == '\0');
            
            if(strncmp(buffer, "begin", 5) == 0 || strncmp(buffer, "Begin", 5) == 0 || strncmp(buffer, "BEGIN", 5) == 0){
                // Se l'utente digita "begin", chiama la funzione begin() per avviare il gioco
                // Gestisce l'entrata in gioco
                ret = begin(sockfd);
                if(ret < 0){
                    close(sockfd);
                    return ret;
                }

                printf("%s: Pronto! \n", user);
                in_game = 1;
                starting = 0;
                print_commands();
            }
            else if(strncmp(buffer, "quit", 4) == 0 || strncmp(buffer, "Quit", 4) == 0 || strncmp(buffer, "QUIT", 4) == 0){
                // Se l'utente digita "quit", chiama la funzione quit() per uscire dal Game Server
                printf("%s: Esco dal Game Server %d \n", user, game_server);
                return quit(sockfd);
            }
            else
                printf("Non hai digitato un comando valido. Riprova. \n");
        }
        else if(in_game != 0 && starting == 0){
            fflush(stdin);
            // Se la partita è iniziata ma non ancora avviata, visualizza messaggi di inizio e setta il flag "starting" a 1
            printf("%s: In gioco \n", user);

            // Riceve le informazioni sulla location di partenza
            // Riceve il nome della location
            ret = recv_msg(sockfd, buffer, 0);
            if(ret < 0){
                close(sockfd);
                return ret;
            }
            printf("%s \n", buffer);

            // Riceve la descrizione della location
            ret = recv_msg(sockfd, buffer, 0);
            if(ret < 0){
                close(sockfd);
                return ret;
            }
            printf("%s \n", buffer);

            memset(buffer, 0, strlen(buffer) + 1);
            starting = 1; // Il turno di inizio è arrivato
        }
        else{
            // La partita è iniziata e si gestiscono le azioni del Client
            memset(buffer, 0, sizeof(buffer));

            // Se la partita è in corso, riceve l'input dell'utente e lo gestisce
            
            get_input_with_timeout(); // Questa funzione è particolare. Controllare bene il suo funzionamento... 

            // Alloca dinamicamente la memoria per il comando e verifica che l'allocazione abbia successo
            comando = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
            if(comando == NULL){
                printf("Errore nella ricezione del comando \n");
                continue;
            }
            strcpy(comando, buffer); // Viene messo il comando digitato dall'utente tramite get_input_with_timeout()
            memset(buffer, 0, sizeof(buffer));
            
            codice = 0;

            // Manda al Main Server un codice per dirgli che è pronto
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nell'invio del codice pronti");
                free(comando);
                return ret;
            }
            
            // Riceve un codice sullo stato della partita
            ret = recv_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nella ricezione del codice di stato");
                free(comando);
                return ret;
            }
            
            if(codice < 0) // Se il codice di stato è negativo, lo restituisce come codice di errore
                return codice;
            else if(codice == 200){ 
                // Se il codice di stato è 200, la partita è terminata in qualche modo
                // Ricevo il risultato della partita 
                free(comando);

                ret = recv_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0){
                    perror("Errore nella ricezione del codice di stato");
                    return ret;
                }

                // Se è 0 si è perso, altrimenti si è vinto
                if(codice == 0)
                    printf("Game Server %d: Non sei riuscito a fuggire! Hai perso! \n", game_server);
                else 
                    printf("Game Server %d: Congratulazioni! Sei fuggito e hai vinto!!!! \n", game_server);

                printf("Game Server %d: In attesa degli altri utenti \n", game_server);
                in_game = 0;

                // Controlla se ci sono ancora messaggi da ricevere
                ret = control_messages(sockfd);

                // Si riceve un codice quando tutti hanno finito. In pratica ci si blocca fino a che il Game Server non ha gestito tutti i giocatori.
                ret = recv_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0){
                    perror("Errore nella ricezione del codice di stato");
                    return ret;
                }

                // Ovviamente se il codice è negativo c'è stato un errore e si restituisce
                if(codice < 0)
                    return codice;
                continue;
            }

            // Chiama la funzione "handle_commands" per gestire i comandi digitati dall'utente tramite "get_input_with_timeout"
            ret = handle_commands(sockfd, comando, game_server, &in_game);
            free(comando);
            if(ret < 0)
                return quit(sockfd);       
        }
    }

    return 0;
}

// Questa funzione mette nel buffer un comando scelto dall'utente, tuttavia se entro un certo tempo non viene inserito viene mandato un timeout
void get_input_with_timeout(){
    // In pratica questa funzione ogni TURN secondi mette la stringa "timeout" all'interno del buffer e la manda al Game Server. 
    // Questo serve affinché il Client e il Game Server comunichino quasi in tempo reale e il Client sia molto spesso aggiornato sulla situazione di gioco. 
    // Se ci sono messaggi pendenti li riceve, se qualcuno digita end lo riceve, o se qualcuno finisce la partita lo riceve. 
    // Tuttavia interrompere a metà un comando da tastiera non crea scompiglio, poiché nel buffer verrano inseriti anche i caratteri digitati prima, 
    // come se non avessimo mai inserito la stringa timeout nel buffer. 
    // Pertanto anche se ci si mette più di TURN secondi a scrivere un comando, non è un problema perché l'integrità del comando è mantenuta. 
    // L'unica cosa scomoda che può succedere è se arriva un messaggio mentre stai scrivendo. A schermo il tuo input e l'output si mescoleranno.
    // Tuttavia rimane l'integrità del comando che si digita e tutto sommato mi sembra un buon compromesso. 

    int flags, pos, c; 
    time_t end = time(0) + TURN; //TURN secondi è il tempo limite per il timeout
    // time(0) è equivalente a time(NULL), ma in questo caso mi piaceva più time(0). Non c'è un vero motivo per lo 0 al posto del NULL. 

    fflush(stdout);

    // Imposta la modalità non bloccante per la lettura da stdin
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    pos = 0;
    while(time(0) < end && pos < BUFFER_SIZE - 1){
        c = getchar();

        // 10 è \n
        if(c != EOF && c != 10 && pos < BUFFER_SIZE - 1) // Ignora EOF e caratteri newline (\n)
            buffer[pos++] = c;

        // Se il carattere inserito è \n, esce dal ciclo
        if(c == 10)
            break;
    }

    buffer[pos] = '\0'; // Aggiunge il terminatore di stringa al buffer

    if(pos <= 0){
        // puts("\nTimeout!\n");  
        // memset(buffer, 0, sizeof(buffer));
        strcpy(buffer, "timeout"); // Se arriva il timeout, viene messa come stringa "timeout" nel buffer
    }

    // Pulisce il buffer di input e ripristina la modalità bloccante per la lettura da stdin
    fcntl(STDIN_FILENO, F_SETFL, flags | ~O_NONBLOCK);
    end = time(0); // Azzera il timer
}

// Questa funzione mette nel buffer una soluzione scelta dall'utente per l'indovinello, tuttavia se entro un certo tempo non viene inserito fallisce
void get_input_for_riddle(){
    // Fa la stessa cosa di "get_input_with_timeout()" ma per l'indovinello

    int flags, pos, c; 
    time_t end = time(0) + RIDDLE_TIME; //RIDDLE_TIME secondi è il tempo limite per il timeout
    // time(0) è equivalente a time(NULL), ma in questo caso mi piaceva più time(0). Non c'è un vero motivo per lo 0 al posto del NULL. 

    fflush(stdout);

    // Imposta la modalità non bloccante per la lettura da stdin
    flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    pos = 0;
    while(time(0) < end && pos < BUFFER_SIZE - 1){
        c = getchar();

        // 10 è \n
        if(c != EOF && c != 10 && pos < BUFFER_SIZE - 1) // Ignora EOF e caratteri newline (\n)
            buffer[pos++] = c;

        // Se il carattere inserito è \n, esce dal ciclo
        if(c == 10)
            break;
    }

    buffer[pos] = '\0'; // Aggiunge il terminatore di stringa al buffer

    if(pos <= 0){
        puts("\nTimeout!\n");  
        strcpy(buffer, "errore"); // Se arriva il timeout, viene messa come stringa "timeout" nel buffer
    }

    // Pulisce il buffer di input e ripristina la modalità bloccante per la lettura da stdin
    fcntl(STDIN_FILENO, F_SETFL, flags | ~O_NONBLOCK);
    end = time(0); // Azzera il timer
}


// Funzione che riceve le informazioni riguardanti un oggetto. Usata principalmente per il comando "take" e per la go_on.
int recv_object(const int sockfd, struct object* o){
    int ret, i = 0, codice;

    // Ricezione del nome dell'oggetto
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;

    // Alloca dinamicamente la memoria per il nome dell'oggetto e verifica se l'allocazione ha successo
    o->name = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(o->name == NULL){
        // Se il nome dell'oggetto non è stato allocato correttamente invia -1
        codice = -1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice");
            return ret;
        }
        return -1;
    }
    strcpy(o->name, buffer);
    
    // Se il nome dell'oggetto è stato allocato correttamente invia 0
    codice = 0;
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // Ricezione della descrizione dell'oggetto
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;

    // Alloca dinamicamente la descrizione per il nome dell'oggetto e verifica se l'allocazione ha successo
    o->description = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(o->description == NULL){
        // Se la descrizione dell'oggetto non è stata allocata correttamente invia -1
        codice = -1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice");
            return ret;
        }
        return -1;
    }
    strcpy(o->description, buffer);

    // Se la descrizione dell'oggetto è stata allocata correttamente invia 0
    codice = 0;
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // Ricezione del tipo dell'oggetto
    ret = recv_all(sockfd, &o->type, sizeof(o->type), 0);
    if(ret < 0){
        perror("Errore nella ricezione del tipo dell'oggetto");
        return ret;
    }

    // Ricezione del tipo dell'enigma
    ret = recv_all(sockfd, &o->enigma.type, sizeof(o->enigma.type), 0);
    if(ret < 0){
        perror("Errore nella ricezione del tipo dell'enigma");
        return ret;
    }

    // Ricezione della domanda dell'enigma
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;

    // Alloca dinamicamente la domanda dell'enigma e verifica se l'allocazione ha successo
    o->enigma.question = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(o->enigma.question == NULL){
        // Se la domanda dell'enigma non è stata allocata correttamente invia -1
        codice = -1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice");
            return ret;
        }
        return ret;
    }
    strcpy(o->enigma.question, buffer);

    // Se la domanda dell'enigma è stata allocata correttamente invia 0
    codice = 0;
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // Ricezione della risposta all'enigma
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;

    // Alloca dinamicamente la risposta all'enigma e verifica se l'allocazione ha successo
    o->enigma.solution = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
    if(o->enigma.solution == NULL){
        // Se la risposta all'enigma non è stata allocata correttamente invia -1
        codice = -1;
        ret = send_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nell'invio del codice");
            return ret;
        }
        return -1;
    }
    strcpy(o->enigma.solution, buffer);

    // Se la risposta all'enigma è stata allocata correttamente invia 0
    codice = 0;
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice");
        return ret;
    }

    // Se l'enigma è a risposta multipla ricevo le risposte multiple
    if(o->enigma.type == 1){
        for(; i < 4; i++){
            // Ricezione dell'opzione
            ret = recv_msg(sockfd, buffer, 0);
            if(ret < 0)
                return ret;

            // Alloco dinamicamente l'opzione e verifico se l'allocazione ha successo
            o->enigma.options[i] = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
            if(o->enigma.options[i] == NULL){
                // Se l'opzione non è stata allocata correttamente invia -1
                codice = -1;
                ret = send_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0){
                    perror("Errore nell'invio del codice");
                    return ret;
                }
                return ret;
            }
            strcpy(o->enigma.options[i], buffer);

            // Se l'opzione è stata allocata correttamente invia 0
            codice = 0;
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nell'invio del codice");
                return ret;
            }
        }

        // Ricevo il numero di tentativi possibili sull'enigma
        ret = recv_all(sockfd, &o->enigma.attempts_left, sizeof(o->enigma.attempts_left), 0);
        if(ret < 0){
            perror("Errore nella ricezione del tipo dell'oggetto");
            return ret;
        }
    }
    else{
        for(; i < 4; i++)
            o->enigma.options[i] = NULL;
        o->enigma.attempts_left = 1;
    }

    // Non servono
    o->usable = -1;
    o->player = -1; 
    o->bonus_points = -1; 
    o->bonus_time = -1; 
    o->opens_door = -1; 
    o->location_index = -1 ; 

    memset(buffer, 0, sizeof(buffer));
    return 0;
}

// Ricevi i messaggi pendenti del Game Server
int control_messages(const int sockfd){
    int ret, codice;
    char* username;
    char* message;

    do{
        // Arriva un codice che indica l'arrivo di un messaggio. 
        // 0 arriva un messaggio, 1 i messaggi da inviare sono finiti (o non ci sono mai stati), -1 errore
        ret = recv_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nella ricezione del codice");
            return ret;
        }

        if(codice == 0){
            // Se il codice è 0 sta arrivando un messaggio
            // Ricezione dello username di colui che ha inviato il messaggio
            ret = recv_msg(sockfd, buffer, 0);
            if(ret < 0)
                return ret;

            // Alloca dinamicamente la memoria per l'username e verifica se l'allocazione ha successo
            username = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
            if(username == NULL){
                // Invia un codice che indica che NON è riuscito ad allocare lo username
                codice = -1;
                ret = send_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0){
                    perror("Errore nell'invio del codice");
                    return ret;
                }
                return codice;
            } 
            strcpy(username, buffer);

            // Invia un codice che indica che è riuscito ad allocare lo username
            codice = 0;
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nell'invio del codice");
                return ret;
            }

            // Ricezione del messaggio in sé e per sé
            ret = recv_msg(sockfd, buffer, 0);
            if(ret < 0)
                return ret;

            // Alloca dinamicamente la memoria per il messaggio e verifica se l'allocazione ha successo
            message = (char*) malloc((strlen(buffer) + 1) * sizeof(char));
            if(message == NULL){
                // Invia un codice che indica che NON è riuscito ad allocare il messaggio
                free(username);
                codice = -1;
                ret = send_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0){
                    perror("Errore nell'invio del codice");
                    return ret;
                }
                return codice;
            } 
            strcpy(message, buffer);

            // Invia un codice che indica che è riuscito ad allocare il messaggio
            codice = 0;
            ret = send_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nell'invio del codice");
                return ret;
            }

            // Riceve un codice che il tipo del messaggio. Se è 0 è un messaggio di sistema, altrimenti è inviato da un utente. 
            ret = recv_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nella ricezione del codice");
                return ret;
            }

            memset(buffer, 0, sizeof(buffer));
            if(codice == 0) // I messaggi di sistema vanno scritti in blu
                printf(ANSI_COLOR_BLUE "%s: %s", username, message);
            else // Quelli degli utenti in verde
                printf(ANSI_COLOR_GREEN "%s: %s", username, message);
            
            printf(ANSI_COLOR_RESET "\n");
            free(username);
            free(message);
            codice = 0;
        }
        else if(codice < 0) // Se il codice è negativo si segnala un errore
            return codice;
    } while(codice != 1); // Se il codice è uguale a 1 non arrivano più messaggi e si può uscire dal ciclo

    return 0;
}

// Comandi

int handle_commands(const int sockfd, char* command, const int game_server, int* in_game){
    int ret;
    const char* none = "none";

    // Invia il comando preso dalla "get_input_with_timeout" al Game Server e gestisce il protocollo in base a tale comando
    
    if(command == NULL)
        printf("Comando non valido!\n");
    else if(strncmp(command, "goto ", 5) == 0)
        return go_to(sockfd, command, game_server);
    else if(strncmp(command, "look", 4) == 0)
        return look(sockfd, command, game_server);
    else if(strncmp(command, "take ", 5) == 0 || strncmp(command, "interact ", 9) == 0)
        return take(sockfd, command, game_server, in_game);
    else if(strcmp(command, "objs") == 0)
        return objs(sockfd, command, game_server);
    else if(strcmp(command, "doors") == 0)
        return doors(sockfd, command, game_server);
    else if(strncmp(command, "use ", 4) == 0)
        return use(sockfd, command, game_server);
    else if(strncmp(command, "message ", 8) == 0)
        return message(sockfd, command, game_server);
    else if(strcmp(command, "help") == 0){
        print_commands();
        ret = send_msg(sockfd, command, strlen(command), 0); // Diciamo al Game Server che si è inserito il comando help
        if(ret < 0)
            return ret;
        return 0;
    }
    else if(strcmp(command, "time") == 0){
        ret = send_msg(sockfd, command, strlen(command), 0);
        if(ret < 0)
            return ret;
        return recv_time(sockfd, game_server);
    }
    else if(strcmp(command, "end") == 0)
        return end(sockfd, command, game_server);
    else if(strcmp(command, "go_on") == 0)
        printf("Comando non valido! go_on non può essere utilizzato così! \n");
    else if(strcmp(command, "timeout") == 0)
        return timeout(sockfd, command, game_server);
    else
        printf("Comando non valido: %s!\n", command);

    // Nel caso non sia stato inserito un comando valido si manda al Game Server la stringa "none"
    ret = send_msg(sockfd, none, strlen(none), 0);
    if(ret < 0)
        return ret;

    return 0;
}

// Comando che inizia la partita
int begin(const int sockfd){
    int ret, codice;
    const char* comando = "begin";

    memset(buffer, 0, strlen(comando));

    // Invia il comando "begin" al Game Server
    ret = send_msg(sockfd, comando, strlen(comando), 0);
    if(ret < 0)
        return ret;

    memset(buffer, 0, 6);

    // Riceve il codice di risposta dal server
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0)
        return ret;
    
    if(codice < 0) // Caso di errore
        return codice;
    else if(codice == 0){
        // Se il codice è 0, aspetta che gli altri Client siano pronti
        printf("%s: Aspetto che gli altri client siano pronti! \n", user);
        ret = recv_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0)
            return ret;
    }
    else if(codice == 1){
        // Se il codice è 1, chiede all'utente se vuole giocare da solo
        printf("Vuoi giocare da solo? [Risposta: sì o no] \n");
        fflush(stdin);
        while(1){
            do{
                memset(buffer, 0, sizeof(buffer));
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = '\0'; // Rimuove il carattere di nuova riga dalla stringa   
            } while(buffer[0] == '\0');
            if(strncmp(buffer, "si", 2) == 0 || strncmp(buffer, "Si", 2) == 0 || strncmp(buffer, "SI", 2) == 0 || strncmp(buffer, "sì", 2) == 0 || strncmp(buffer, "Sì", 2) == 0 || strncmp(buffer, "SÌ", 2) == 0){
                // Se "sì" parte la partita in Single-Player
                codice = 1;
                ret = send_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0)
                    return ret;
                
                ret = recv_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0)
                    return ret;

                break;
            }
            else if(strncmp(buffer, "no", 2) == 0 || strncmp(buffer, "No", 2) == 0 || strncmp(buffer, "NO", 2) == 0){
                // Se no aspetta che arrivino altri Client e che siano pronti
                codice = 0;
                ret = send_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0)
                    return ret;
                
                ret = recv_all(sockfd, &codice, sizeof(codice), 0);
                if(ret < 0)
                    return ret;
                
                break;
            }
            else
                printf("Devi rispondere sì o no. \n");
        }
        
        if(codice < 0)
            return codice;
    }

    memset(buffer, 0, sizeof(buffer));
    fflush(stdin);

    return 0;
}

// Comando che fa uscire dal Game Server
int quit(const int sockfd){
    int ret, codice = 0, risultato = -1;
    const char* comando = "quit";

    memset(buffer, 0, strlen(comando));

    // Invia il comando "quit" al server
    ret = send_msg(sockfd, comando, strlen(comando), 0);
    if(ret < 0)
        return ret;
    memset(buffer, 0, 5);

    // Riceve il codice di risposta del Game Server
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }
    
    // Se il codice è negativo esce
    if(codice < 0)
        return codice;
    
    // Chiede all'utente se vuole uscire anche dal Main Server
    printf("Vuoi uscire anche dal Main Server (e quindi dal gioco)? [Risposta: sì o no] \n");
    fflush(stdin);
        while(1){
            do{
                memset(buffer, 0, sizeof(buffer));
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = '\0'; // Rimuove il carattere di nuova riga dalla stringa   
            } while(buffer[0] == '\0');
            
            if(strncmp(buffer, "si", 2) == 0 || strncmp(buffer, "Si", 2) == 0 || strncmp(buffer, "SI", 2) == 0 || strncmp(buffer, "sì", 2) == 0 || strncmp(buffer, "Sì", 2) == 0 || strncmp(buffer, "SÌ", 2) == 0){
                risultato = 0;
                break;
            }
            else if(strncmp(buffer, "no", 2) == 0 || strncmp(buffer, "No", 2) == 0 || strncmp(buffer, "NO", 2) == 0){
                risultato = -1;
                break;
            }
            else
                printf("Devi rispondere sì o no. \n");
        }

    // Chiude il socket e restituisce il risultato
    close(sockfd);
    return risultato;
}

// Comando che fa andare l'utente in una nuova location
int go_to(const int sockfd, char* msg, const int game_server){
    int ret, codice;

    // Si invia la stringa "goto [location]"
    ret = send_msg(sockfd, msg, strlen(msg), 0);
    if(ret < 0)
        return ret;

    // Si riceve un codice di risposta dal Game Server
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0)
        return ret;

    if(codice == 0) // La location esiste
        ret = look(sockfd, "look", game_server); // Si chiedono le informazioni sulla stanza
    else if(codice == -1){ 
        // La location non esiste
        printf("Game Server %d: Location non valida...\n", game_server); 
        ret = recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi
    }
    else if(codice == 1){
        // La location esiste, ma non vi si può accedere
        printf("Game Server %d: Location bloccata...\n", game_server);
        ret = recv_time(sockfd, game_server);
    }
    else
        ret = recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi
        
    return ret;
}

// Comando che permette all'utente di avere la descrizione della sua attuale location o di un oggetto presente in tale location
int look(const int sockfd, const char* msg, const int game_server){
    int ret, i = 0, codice;

    // Si invia o la stringa "look" o "look [object]"
    ret = send_msg(sockfd, msg, strlen(msg), 0);
    if(ret < 0)
        return ret;

    // Si riceve un codice di risposta dal Game Server
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0)
        return ret;

    // Se il codice è negativo il comando era errato
    if(codice < 0){
        printf("Comando non valido! \n");
        return 0;
    }

    // Si riceve il nome della location o dell'oggetto
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;
    printf("%s\n", buffer);

    // Si riceve la descrizione della location o dell'oggetto
    ret = recv_msg(sockfd, buffer, 0);
    if(ret < 0)
        return ret;
    printf("%s\n", buffer);

    // Si riceve un ulteriore Codice dal Game Server 
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0)
        return ret;

    // Se è uno si trova in una location e quindi riceverà le informazioni sugli utenti nella location
    if(codice == 1){
        // Riceve il numero di utenti presenti nella location
        ret = recv_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0)
            return ret;

        // Riceve gli username degli utenti presenti nella location
        if(codice > 0){
            printf("\nIn questa location sono presenti: \n");
            for(; i < codice; i++){
                ret = recv_msg(sockfd, buffer, 0);
                if(ret < 0)
                    return ret;

                printf("%s\n", buffer);
            }
        }
    }

    return recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi
}

// Comando per interagire o prendere un oggetto
int take(const int sockfd, const char* msg, const int game_server, int* in_game){
    int ret, codice;
    struct object* o;

    // Si invia la stringa "take object"
    ret = send_msg(sockfd, msg, strlen(msg), 0);
    if(ret < 0)
        return ret;

    // Si riceve un codice di risposta dal Game Server
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0)
        return ret;

    // Gestiamo a seconda del codice
    if(codice == 0){
        // L'oggetto è presente
        o = (struct object*) malloc(sizeof(struct object));
        // Ci vengono inviate le informazioni sull'oggetto
        ret = recv_object(sockfd, o);
        if(ret < 0){
            printf("Fallimento nel recuperare l'oggetto \n");
            return ret;
        }
            
        printf("Game Server %d: Hai interagito con **%s**\n", game_server, o->name);
        printf("%s \n", o->description);

        return go_on(sockfd, o, game_server, in_game); // Bisogna indovinare l'indovinello per prendere l'oggetto
    }
    else if(codice == 1){
        // Ricevo il nome dell'oggetto perché l'ho raccolto
        ret = recv_msg(sockfd, buffer, 0);
        if(ret < 0)
            return ret;
        
        printf("Game Server %d: Hai raccolto l'oggetto **%s**\n", game_server, buffer);
    }
    else if(codice < 0){
        // Se il codice è negativo c'è stato un errore
        if(codice == -1)
            printf("Game Server %d: L'oggetto inviato o non è valido o non è presente nella location\n", game_server);
        else if(codice == -2)
            printf("Game Server %d: Hai troppi oggetti nell'inventario \n", game_server);
        else if(codice == -3){ // In questo caso ci sta lavorando un altro utente e riceviamo il suo username
            ret = recv_msg(sockfd, buffer, 0);
            printf("Game Server %d: Su questo oggetto ci sta lavorando %s\n", game_server, buffer);
        }
        else if(codice == -4)
            printf("Game Server %d: Serve un oggetto per poterci interagire \n", game_server);
        else if(codice == -5)
            printf("Game Server %d: Non hai abbastanza punti per interagire con la stanza \n", game_server);
    }

    return recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi
}

// Questo prosegue il comando "take" e permette di risolvere l'indovinello dell'oggetto
int go_on(const int sockfd, struct object* object, const int game_server, int* in_game){
    int ret, i = 0, risultato = 0, codice;
    const char* go_on = "go_on";

    memset(buffer, 0, sizeof(buffer));
    fflush(stdin);

     // Gestisce l'indovinello dell'oggetto
    if(object->enigma.type == 0){
        // Gestisce l'indovinello a risposta aperta
        printf("%s\n", object->enigma.question);
        printf("Digita la soluzione all'indovinello:  ");
        get_input_for_riddle();
        /*
        do{
            memset(buffer, 0, sizeof(buffer));
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = '\0'; // Rimuove il carattere di nuova riga dalla stringa   
        } while(buffer[0] == '\0');
        */

        // Risultato è già a 0, sicché se non si indovina non c'è bisogno di settarlo
        if(strcmp(object->enigma.solution, buffer) == 0){
            printf("Complimenti hai indovinato! Adesso puoi prendere l'oggetto **%s**. \n", object->name);
            risultato = 1;
        }
        else 
            printf("Hai sbagliato. \n");
    }
    else if(object->enigma.type == 1){
        // Gestisce l'indovinello a risposta multipla
        printf("%s\n", object->enigma.question);
        printf("Le opzioni sono: \n");

        for(i = 0; i < 4; i++)
            printf("%s\n", object->enigma.options[i]);
            
        printf("Digita la soluzione all'indovinello:  ");
        get_input_for_riddle();
        /*
        do{
            memset(buffer, 0, sizeof(buffer));
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = '\0'; // Rimuove il carattere di nuova riga dalla stringa   
        } while(buffer[0] == '\0'); 
        */

        // Risultato è già a 0, sicché se non si indovina non c'è bisogno di settarlo
        if(strcmp(object->enigma.solution, buffer) == 0){
            risultato = 1;
            printf("Complimenti hai indovinato! Adesso puoi prendere l'oggetto **%s**. \n", object->name);
        }
        else{
            object->enigma.attempts_left--;
            if(risultato == 0 && object->enigma.attempts_left > 0)
                printf("Errato. Riprova. \n\n");   
            else if(risultato == 0) // In questo caso si perde direttamente
                printf("Non hai indovinato. Hai perso! \n");

            memset(buffer, 0, sizeof(buffer));
        }                        
    }

    // Questa parte serve perché dobbiamo comunicare al Game Server di essere pronti, poiché dopo aver ricevuto l'oggetto lasciamo che il Game Server prosegua, 
    // altrimenti sarebbe vincolato ad aspettare una nostra risposta e gli altri Client sarebbero bloccati ad aspettare che noi inviamo la risposta, 
    // potenzialmente bloccando il gioco. 

    // Quindi qua inviamo un codice per indicare che siamo pronti a comunicare col Game Server
    codice = 0;
    ret = send_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nell'invio del codice pronti");
        return ret;
    }

    // Riceviamo lo stato del Game Server   
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice di stato");
        return ret;
    }

    if(codice < 0) // Se è minore di 0 si è verificato un errore
        return codice;
    else if(codice == 200){
        // La partita è finita
        // Riceve un codice che indica se la partita è stata vinta o meno
        ret = recv_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nella ricezione del codice di stato");
            return ret;
        }

        if(codice == 0) // Partita persa
            printf("Game Server %d: Non sei riuscito a fuggire! Hai perso! \n", game_server);
        else // Partita vinta
            printf("Game Server %d: Congratulazioni! Sei fuggito e hai vinto!!!! \n", game_server);

        printf("Game Server %d: In attesa degli altri utenti \n", game_server);
        // La variabile in_game, presente all'interno della funzione che gestisce il gioco "game_escape_room" deve essere messa a 0 per indicare che 
        // la partita è finita
        *in_game = 0;

        // Controlli se ci sono messaggi pendenti
        ret = control_messages(sockfd);

        // Ricevo il codice di fine e potenzialmente mi blocco ad aspettare che gli altri Client finiscano
        ret = recv_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nella ricezione del codice di stato");
            return ret;
        }
        if(codice < 0)
            return codice;
        
        return 0; // Ritorno 0 e finisco il gioco
    }

    // Se invece il codice non è negativo e non è 200 proseguo normalmente

    // Invio la stringa "go_on" al Game Server
    ret = send_msg(sockfd, go_on, strlen(go_on), 0);
    if(ret < 0)
        return ret;

    // Invio il risultato 
    ret = send_all(sockfd, &risultato, sizeof(risultato), 0);
    if(ret < 0){
        perror("Errore nell'invio del risultato");
        return ret;
    }

    // Mando il nome dell'oggetto di cui ho risolto o provato a risolvere l'indovinello
    ret = send_msg(sockfd, object->name, strlen(object->name), 0);
    if(ret < 0)
        return ret;

    // Ricevo un codice che mi dice se l'operazione è andata a buon fine
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }

    // Se il codice è negativo l'operazione non è andata a buon fine e si ritorna l'errore
    if(codice < 0)
        return codice;

    return recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi
}

// Comando per usare un oggetto
int use(const int sockfd, const char* msg, const int game_server){
    int ret, codice, tempo_bonus;

    // Invio una stringa del tipo "use object" o "use object1 object2"
    ret = send_msg(sockfd, msg, strlen(msg), 0);
    if(ret < 0)
        return ret;

    // Ricevo un codice per vedere se l'invio della stringa è andato a buon fine
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }

    if(codice < 0) // C'è stato un errore e pertanto lo restituisco
        return codice;
    else if(codice == 0){
        // Ho inviato una stringa di tipo "use object"
        // Ricevo un codice che mi indica se ho usato bene l'oggetto
        ret = recv_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nella ricezione del codice");
            return ret;
        }

        if(codice == 0){
            // L'oggetto è stato usato correttamente
            
            // Ricevo il tipo dell'oggetto
            ret = recv_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nella ricezione del codice");
                return ret;
            }

            // Ricevo il tempo guadagnato
            ret = recv_all(sockfd, &tempo_bonus, sizeof(tempo_bonus), 0);
            if(ret < 0){
                perror("Errore nella ricezione del codice");
                return ret;
            }

            if(codice != 2){
                if(tempo_bonus == 0)
                    printf("Game Server %d: L'oggetto è stato usato con successo. \n", game_server);
                else
                    printf("Game Server %d: L'oggetto è stato usato con successo. Hai ottenuto %d secondi!\n", game_server, tempo_bonus);
            }
            else{
                tempo_bonus *= -1; // Lo moltiplico per -1, perché se è 0 non ho conseguenze, altrimenti mi serve di stamparlo positivo
                printf("Game Server %d: L'oggetto è stato usato con successo. Un momento ... oh no! Era una trappola. Hai perso %d secondi!\n", game_server, tempo_bonus);
            }
        }
        else if(codice == -1) // Errore nella ricerca o oggetto non presente
            printf("Game Server %d: Errore nella ricerca. L'oggetto potrebbe non essere presente nell'inventario\n", game_server);
        else if(codice == 1) // L'oggetto non può essere usato in tal modo
            printf("Game Server %d: L'oggetto selezionato così non si può usare in questo modo\n", game_server);
    }
    else if(codice == 1){
        // Ho inviato una stringa di tipo "use object1 object2"
        // Ricevo un codice che mi indica se ho usato bene l'oggetto
        ret = recv_all(sockfd, &codice, sizeof(codice), 0);
        if(ret < 0){
            perror("Errore nella ricezione del codice");
            return ret;
        }

        if(codice == -1) // Caso in cui il primo oggetto object1 non si trovi
            printf("Game Server %d: Il primo oggetto non è stato trovato nell'inventario\n", game_server);
        else if(codice == 0){
            // Caso in cui il primo oggetto object1 sia stato trovato nell'inventario
            // Ricevo un codice per il secondo oggetto object2
            ret = recv_all(sockfd, &codice, sizeof(codice), 0);
            if(ret < 0){
                perror("Errore nella ricezione del codice");
                return ret;
            }
            if(codice == -1) // Casco in cui il secondo oggetto object2 non è stato trovato nell'attuale location
                printf("Game Server %d: Il secondo oggetto non è stato trovato nella location\n", game_server);
            else if(codice == 0){
                // Caso in cui il secondo oggetto object2 è stato trovato nell'attuale location
                ret = recv_msg(sockfd, buffer, 0);
                if(ret < 0)
                    return ret;

                printf("Game Server %d: Ottimo lavoro. Sembra che questa fosse la soluzione. Hai ottenuto **%s**! Adesso lo puoi prendere.\n", game_server, buffer);
                memset(buffer, 0, strlen(buffer) + 1);
            }
            else if(codice == 1) // Caso in cui il secondo oggetto object2 è stato trovato nell'attuale location, ma si è sbagliato nella scelta di object1
                printf("Game Server %d: Il secondo oggetto non può avere questa soluzione\n", game_server);
        }
        else if(codice == 1) // Caso in cui il primo oggetto object1 sia stato trovato nell'inventario, ma non si può usare così
            printf("Game Server %d: Il primo oggetto non si può usare in questo modo\n", game_server);
    }

    return recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi
}

// Comando che restituisce gli oggetti presenti nell'inventario
int objs(const int sockfd, const char* msg, const int game_server){
    int ret, i = 0, oggetti;

    // Invio la stringa "objs" al Game Server
    ret = send_msg(sockfd, msg, strlen(msg), 0);
    if(ret < 0)
        return ret;

    // Ricevo il numero di oggetti
    ret = recv_all(sockfd, &oggetti, sizeof(oggetti), 0);
    if(ret < 0){
        perror("Errore nella ricezione del numero di oggetti");
        return ret;
    }

    // Se è minore o uguale a 0 non stampo nulla
    if(oggetti <= 0)
        return recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi

    for(; i < oggetti; i++){
        // Ricevo il nome dell'oggetto
        ret = recv_msg(sockfd, buffer, 0);
        if(ret < 0 )
            return ret;
        // Stampo il nome dell'oggetto
        printf("%s\n", buffer);
    }

    memset(buffer, 0, sizeof(buffer));

    return recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi
}

// Comando che mi indica a quali location posso accedere dalla mia attuale location
int doors(const int sockfd, const char* msg, const int game_server){
    int ret, i = 0, porte, chiusura;

    // Invio la stringa "doors" al Game Server
    ret = send_msg(sockfd, msg, strlen(msg), 0);
    if(ret < 0)
        return ret;

    // Ricevo il numero di location a cui posso accedere
    ret = recv_all(sockfd, &porte, sizeof(porte), 0);
    if(ret < 0){
        perror("Errore nella ricezione del numero di oggetti");
        return ret;
    }
    
    // Se è minore o uguale a 0 non stampo nulla
    if(porte <= 0)
        return recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi

    for(; i < porte; i++){
        // Ricevo il nome della porta
        ret = recv_msg(sockfd, buffer, 0);
        if(ret < 0 )
            return ret;

        // Ricevo un codice che mi dice se è chiusa
        ret = recv_all(sockfd, &chiusura, sizeof(chiusura), 0);
        if(ret < 0){
            perror("Errore nella ricezione delle informazioni riguardo all'accessibilità delle locazioni");
            return ret;
        }

        if(chiusura == 0) // Non è chiusa
            printf("%s\n", buffer);
        else // È chiusa
            printf("%s [bloccata] \n", buffer);
    }

    memset(buffer, 0, sizeof(buffer));

    return recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi
}

// Comando che invia un messaggio ad altri utenti nella stessa location
int message(const int sockfd, const char* msg, const int game_server){
    int ret, codice;
    char* stringa;
    char* posizione;

    // Invio la stringa "message [phrase]" al Game Server
    ret = send_msg(sockfd, msg, strlen(msg), 0);
    if(ret < 0)
        return ret;

    // Ricevo un codice 
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }
    
    if(codice < 0) // Se è minore di 0 si è verificato un errore
        printf("Game Server %d: Messaggio non inviato \n", game_server);
    else{
        posizione = strstr(msg, "message ");
        // Avanza il puntatore oltre "message "
        posizione += 8;

        // Ottieni la stringa desiderata
        stringa = strdup(posizione);
        // Se no è stato inviato correttamente e quindi viene stampato
        printf("Game Server %d: Messaggio inviato! \n", game_server);
        printf(ANSI_COLOR_GREEN "%s: %s", user, stringa);
        printf(ANSI_COLOR_RESET "\n");
        free(stringa);
    }

    return recv_time(sockfd, game_server); // Ricevo le informazioni sul tempo rimanente e sui punteggi
}

// Comando che stampa quali comandi sono leciti
int help(){
    print_commands();
    return 0;
}

// Comando che finisce la partita per tutti i giocatori
int end(const int sockfd, const char* msg, const int game_server){
    int ret, codice;

    // Invio la stringa "end" al Game Server
    ret = send_msg(sockfd, msg, strlen(msg), 0);
    if(ret < 0)
        return ret;

    // Ricevo un codice
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }
        
    if(codice < 0) // Se è minore di 0 restituisco l'errore
        return codice;
    
    printf("Attendere che il Game Server finisca la partita ... \n");

    return 0;
}

// Ricevo le informazioni sul tempo rimanente e sui punteggi degli altri utenti
int recv_time(const int sockfd, const int game_server){
    int ret, i = 0, minuti, secondi, giocatori, punteggio, codice;

    printf("\n");

    // Ricevo i minuti rimanenti
    ret = recv_all(sockfd, &minuti, sizeof(minuti), 0);
    if(ret < 0){
        perror("Errore nella ricezione dei minuti");
        return ret;
    }

    // Ricevo i secondi rimanenti
    ret = recv_all(sockfd, &secondi, sizeof(secondi), 0);
    if(ret < 0){
        perror("Errore nella ricezione dei secondi");
        return ret;
    }

    if(minuti < 0)
        minuti = 0;

    if(secondi < 0)
        secondi = 0;

    printf("Tempo rimanente: %d minuti e %d secondi\n", minuti, secondi);

    // Ricevo il numero di giocatori attivi
    ret = recv_all(sockfd, &giocatori, sizeof(giocatori), 0);
    if(ret < 0){
        perror("Errore nella ricezione del numero dei giocatori");
        return ret;
    }

    for(; i < giocatori; i++){
        // Ricevo il loro username
        ret = recv_msg(sockfd, buffer, 0);
        if(ret < 0)
            return ret;
        
        // Ricevo il loro punteggio
        ret = recv_all(sockfd, &punteggio, sizeof(punteggio), 0);
        if(ret < 0){
            perror("Errore nella ricezione del punteggio");
            return ret;
        }

        printf("Punteggio di %s: %d\n", buffer, punteggio);
    }

    printf("\n");

    // Ricevo un codice di conclusione
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }

    if(codice == 1) // Se è 1 potrei riceve messaggi pendenti
        ret = control_messages(sockfd);

    return 0;
}


void print_commands(){
    printf(ANSI_COLOR_RED "I comandi ammessi durante il gioco sono:\n");
    printf("goto [location] -> Vai alla location [location]\n");
    printf("look -> Fornisce una breve descrizione della locazione in cui ti trovi\n");
    printf("look [object] -> Fornisce una breve descrizione dell'oggetto [object] presente nella location corrente\n");
    printf("take [object] -> Consente al giocatore di raccogliere un oggetto presente nella stanza corrente. Se l'oggetto è bloccato, il giocatore riceve un enigma da risolvere.\n");
    printf("interact [object] -> Consente al giocatore di raccogliere un oggetto presente nella stanza corrente. Se l'oggetto è bloccato, il giocatore riceve un enigma da risolvere.\n");
    printf("use [object] -> Permette al giocatore di utilizzare un oggetto [object] che hai nell'inventario\n");
    printf("use [object1] [object2] -> Permette al giocatore di utilizzare un oggetto [object1] che hai nell'inventario. Se il comando contiene anche il parametro [object2], l'oggetto [object1] viene usato con l'oggetto [object2] per risolvere un enigma.\n");
    printf("objs -> Mostra l'elenco degli oggetti raccolti fino a quel momento.\n");
    printf("doors -> Mostra l'elenco delle location a cui puoi raggiungere.\n");
    printf("message [phrase] -> Invia un messaggio [phrase] a tutti quelli che si trovano nella stessa location\n");
    printf("time -> Stampa il tempo rimanente e i punti dei giocatori\n");
    printf("help -> Stampa la lista dei comandi\n");
    printf("end -> Termina il gioco\n");
    printf(ANSI_COLOR_RESET "\n");
}

// Timeout chiamato dalla "get_input_with_timeout"
int timeout(const int sockfd, const char* timeout, const int game_server){
    int ret, codice;

    // Invio la stringa timeout
    ret = send_msg(sockfd, timeout, strlen(timeout), 0);
    if(ret < 0)
        return ret;
    
    // Ricevo il codice di stato
    ret = recv_all(sockfd, &codice, sizeof(codice), 0);
    if(ret < 0){
        perror("Errore nella ricezione del codice");
        return ret;
    }

    if(codice < 0)
        return codice;
    else if(codice == 200)
        return 0;
    else if(codice == 1){
        // I messaggi sono ricevuti più o meno in tempo reale con un delay di circa TURN secondi
        ret = control_messages(sockfd);
        return ret;
    }

    return 0;
}

// FINE PROGRAMMA