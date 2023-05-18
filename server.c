#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/select.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<time.h>
#include<stdbool.h>
#include<signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include"./utility/UserEntry.h"
#include"./utility/TCP.h"
#include"./utility/UserPending.h"

#define DEBUG_ON

#ifdef DEBUG_ON
    #define DEBUG_PRINT(x) printf("[DEBUG]: "); printf x; printf("\n"); fflush(stdout);
#else
    #define DEBUG_PRINT(x) do{} while(0)
#endif

#define OP_LEN 4
#define BUFFER_LEN 1024
#define USER_LEN 30
#define PASSWORD_LEN 30


/*
    Server_struct è la struttura dati utilizzata per tenere conto delle 
    informazioni più importanti riguardanti il server
*/
struct Server_struct {
    int port;                   //porta su cui il server è in ascolto
    UserEntry* user_registrati; //lista degli utenti registrati
    UserPending* user_pending;  //lista degli utenti che devono ricevere un msg dal server, ma che sono offline
    char* user_file;            //nome del file contenente le credenziali degli utenti per l'utenticazione
    char* user_pending_file;    //nome del file contenete le notifiche per l'avvenuta lettura che non sono ancora state ricevute
} Server = {4242,   //port
    NULL,           //user_registrati
    NULL,           //user_pending
    "user.txt",      //user_file
    "UserPending.txt" //user_pending_file
};

/*
    file_descriptor_set è la struttura dati che serve per gestire
    i file_descriptor per la funzione di IOMultiplexing
*/
struct file_descriptor_set{
    fd_set master;
    fd_set read_fds;
    int fdmax;
} fds;


/*
    caricaUserDaFile è la funzione che legge da file le informazio degli utenti
    registrati, questa funzione salva le informazioni trovate in Server.user_registrati
    sfrutta una funzione di utilita di UserEntry.h per effettuare l'inserimento in coda
    dell'elemento nella lista
*/
void caricaUserDaFile(){
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    fp = fopen(Server.user_file, "r");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", Server.user_file));
    } else {

        //se il file viene aperto in lettura senza problemi
        //lo scorro riga per riga e sfruttando un puntatore di appoggio
        //(this_user) leggo i dati dal file e li inserisco prima in 
        //this_user e poi in Server.user_registrati
        while ((read = getline(&line, &len, fp)) != -1) {
            UserEntry* this_user = malloc(sizeof(UserEntry));
            DEBUG_PRINT(("%s\n", line));
            if (!sscanf(line, "%s %s %ld %ld %ld %d",
                        this_user->user_name, 
                        this_user->password, 
                        &this_user->port,
                        &this_user->timestamp_login, 
                        &this_user->timestamp_logout,
                        &this_user->tentativi_connessione) 
            ){
                DEBUG_PRINT(("errore nel parsing di un record nel file %s", Server.user_file));
            } else {
                this_user->next = NULL;
                inserisciUser(this_user, &Server.user_registrati);
            }
        }
    fclose(fp);
    if (line)
        free(line);
    }
}

/*
    salvaUserSuFile è la funzione che dato il contenuto di Server.user_registrati
    lo salva su file in seguito a delle modifiche
*/
void salvaUserSuFile(){
    FILE* fp;
    fp = fopen(Server.user_file, "w");
    if (fp == NULL) {
        DEBUG_PRINT(("impossibile aprire file: %s", Server.user_file));
        return;
    }

    //se il file viene aperto in lettura senza problemi, scorro
    //Server.user_registrati e scrivo i suoi elementi su file,
    //per ogni passaggio libero la memoria allocata puntata da 
    //Server.user_registrati
    UserEntry* elem;
    while(Server.user_registrati) {
        elem = Server.user_registrati;
        fprintf(fp, "%s %s %d %ld %ld %d\n", elem->user_name, 
                                        elem->password, 
                                        elem->port,
                                        elem->timestamp_login, 
                                        elem->timestamp_logout,
                                        elem->tentativi_connessione);
        Server.user_registrati = elem->next;
        free(elem);
    }
    fclose(fp);
}

/*
    caricaUserPendingDaFile è la funzione che legge da file le notifiche per l'avvenuta
    lettura, questa funzione salva le informazioni trovate in Server.user_pending
    sfrutta una funzione di utilita di UserPending.h per effettuare l'inserimento in coda
    dell'elemento nella lista
*/
void caricaUserPendingDaFile(){
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    fp = fopen(Server.user_pending_file, "r");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", Server.user_file));
    } else {

        //se il file viene aperto in lettura senza problemi
        //lo scorro riga per riga e sfruttando un puntatore di appoggio
        //(this_user) leggo i dati dal file e li inserisco prima in 
        //this_user e poi in Server.user_registrati
        while ((read = getline(&line, &len, fp)) != -1) {
            UserPending* this_user = malloc(sizeof(UserPending));
            DEBUG_PRINT(("%s\n", line));
            if (!sscanf(line, "%s %s %d",
                        this_user->username_sender, 
                        this_user->username_receiver, 
                        &this_user->port_sender) 
            ){
                DEBUG_PRINT(("errore nel parsing di un record nel file %s", Server.user_file));
            } else {
                this_user->next = NULL;
                inserisciUserPending(this_user, &Server.user_pending);
            }
        }
    fclose(fp);
    if (line)
        free(line);
    }
}

/*
    salvaUserPendingSuFile è la funzione che dato il contenuto di Server.user_pending
    lo salva su file in seguito a delle modifiche
*/ 
void salvaUserPendingSuFile(UserPending* elem){
    FILE* fp;
    fp = fopen(Server.user_pending_file, "w");
    if (fp == NULL) {
        DEBUG_PRINT(("impossibile aprire file: %s", Server.user_file));
        return;
    }

    //se il file viene aperto in scrittura senza problemi, scorro
    //Server.user_pending e scrivo i suoi elementi su file,
    //per ogni passaggio libero la memoria allocata puntata da
    UserPending* tmp;
    while(elem) {
        tmp = elem;
        fprintf(fp, "%s %s %d\n", tmp->username_sender, tmp->username_receiver, tmp->port_sender);
        elem= elem->next;
        free(tmp);
    }
    fclose(fp);
}

/*
    eliminaUserPendig è la funzione che elimina l'elemento elem
    dalla lista lista_bersaglio passata in ingresso
*/

void eliminaUserPending(UserPending* elem, UserPending **lista_bersaglio){
    UserPending* tmp = *lista_bersaglio;
    UserPending* follower = *lista_bersaglio;
    
    //se l'emento che voglio eliminare è il primo, semplicemente aggiorno Server.user_pending
    if(!strcmp(tmp->username_sender, elem->username_sender) && !strcmp(tmp->username_receiver, elem->username_receiver)){
       Server.user_pending = Server.user_pending->next;
       free(tmp);
    return;
    }

    //altrimenti scorro lista_bersaglio finché non trovo elem o raggiung l'ultimo elemento;
    while(tmp->next != NULL) {
        if(!strcmp(tmp->username_sender, elem->username_sender) && !strcmp(tmp->username_receiver, elem->username_receiver)){
            break;
        }
        follower = tmp;
        tmp = tmp->next;
    }
    if(tmp!=NULL){
        follower=tmp->next;
        free(tmp);
    }    
}

/*
    liberaUser si occupa di liberare la memoria puntata da
    Server.user_registrati, questa funzione viene invocata ogni 
    volta che viene invocata la funzione di caricaUserDaFile
    in quanto è meglio ricaricare da file le informazioni sugli utenti
    ogni volta che si ha bisogno piuttosto che basarsi su dati precedentemente 
    caricati, ma che potrebbero non essere più validi
*/
void liberaUser(){
    UserEntry* elem;
    while(Server.user_registrati != NULL){
        elem = Server.user_registrati;
        Server.user_registrati = Server.user_registrati->next;
        free(elem);
    }
}

/*
    mostraUserOnline è la funzione che si occupa di stampare a video
    il nome, il timestamp del login e la porta degli utenti attualmente online.
    Prende in ingresso una porta che identifica il processo che fa la richiesta 
    di mostrare gli user attualmente online. Se port vale 0 vuol dire che la richiesta è
    stata fatta in locale da terminale, altrimenti identifica un device esterno a cui
    bisogna inviare la lista degli utenti online
*/
void mostraUserOnline(int port){
    int newsd;
    int users = 0;
    int users_online = 0;
    UserEntry* elem;
    caricaUserDaFile();

    //dopo aver caricato gli user da file, scorro Server.user_registrati e
    //per ogni elemento della lista controllo se l'utente è online e se si
    //stampo a video nome, timestamp login e porta
    for (elem = Server.user_registrati; elem != NULL; elem = elem->next) {
        
        //se port è uguale a zero servo la richiesta locale stampando a video
        if(port == 0){           
            users++;
            DEBUG_PRINT(("%s\n", elem->user_name));
            if(elem->timestamp_login > elem->timestamp_logout) {
                users_online++;
                printf("%s*", elem->user_name);
                printf("%s*",ctime(&elem->timestamp_login));
                printf("%d\n", elem->port);
                fflush(stdout);
            }
        }

        //altrimenti invio al device in ascolto su port il nome degli user online
        else{
            if(elem->timestamp_login > elem->timestamp_logout) {
                newsd = creaConnessioneTCP(port);
                inviaMessaggioTCP(newsd, "UON", (void*)elem->user_name, strlen(elem->user_name));
            }
        }
    }
    liberaUser();
    if(port == 0)
        printf("%d utenti connessi su %d utenti registrati\n", users_online, users);
}

/*
    controllaCrendenzialiSignup è la funzione che controlla se un utente
    identificato da username, password e porta che prende in ingresso
    è già presente o meno nel file degli utenti. Se è già presente, la funzione
    ritorna false, altrimenti aggiunge una nuova entrata a Server.user_registrati
    e salva il suo contenuto su file di testo
*/
bool controllaCrendenzialiSignup(char* username, char* password, int port){
    UserEntry* elem;
    struct timeval tv;
    caricaUserDaFile();
    for ( elem = Server.user_registrati; elem!=NULL; elem = elem->next) {
        if (!strcmp(username, elem->user_name))
            return false;
    }
    UserEntry* this_user = malloc(sizeof(UserEntry));
    this_user->next = NULL;
    this_user->port = port;
    sscanf(username, "%s", this_user->user_name);
    sscanf(password, "%s", this_user->password);
    inserisciUser(this_user, &Server.user_registrati);
    DEBUG_PRINT(("credenziali caricate: %s %s", this_user->user_name, this_user->password));
    salvaUserSuFile();
    return true;
}

/*
    controllaCrendenzialiIn è la funzione che controlla le credenziali
    degli utenti registrati e le confronta con quelle ricevute in ingresso,
    se il confronto ha successo la funzione ritorna true, false altrimenti
*/
bool controllaCrendenzialiIn(char* username, char* password, int port){
    UserEntry* elem;
    struct timeval tv;
    caricaUserDaFile();
    for ( elem = Server.user_registrati; elem!=NULL; elem = elem->next) {
        if (!strcmp(username, elem->user_name) && !strcmp(password, elem->password)){

            //se il confronto ha successo, aggiorno il timestamp del logini dell'utente
            //che ha fatto la richiesta di login, resetto il timestamp di logout
            //e inserisco la porta su cui è in ascolto il device richiedente
            //infine salvo la modifica su file
            DEBUG_PRINT(("%s %s -- %s %s", username, password, elem->user_name, elem->password));
            gettimeofday(&tv, NULL);
            elem->timestamp_login = tv.tv_sec;
            elem->timestamp_logout = 0;
            elem->port = port;
            salvaUserSuFile();
            return true;
        }
    }
    liberaUser();
    return false;
}

/*
    logoutUser è la funzione che si occupa di aggiornare il 
    timestamp logout di un utente identificato dalla porta su cui si trova in
    ascolto che la funzione prende in ingresso. Se l'operazione ha successo, 
    funzione ritorna true, false altrimenti
*/
bool logutUser(int port){
    UserEntry* elem;
    struct timeval tv;
    caricaUserDaFile();
    for ( elem = Server.user_registrati; elem!=NULL; elem = elem->next) {
        if (elem->port == port){
            //se il confronto ha successo, aggiorno il timestamp lougout
            //e salvo le modifiche su file
            gettimeofday(&tv, NULL);
            elem->timestamp_logout = tv.tv_sec;
            salvaUserSuFile();
            return true;
        }
    }
    liberaUser();
    return false;
}

/*
    aggiornaUserOffline è la funzione che aggiorna il timestamp logout
    di un utente passato in ingresso alla funzione
*/
void aggiornaUserOffline(UserEntry* user_offline){
    UserEntry* elem;
    struct timeval tv;
    for(elem = Server.user_registrati; elem!=NULL; elem=elem->next){
        if(!strcmp(elem->user_name, user_offline->user_name)){
            gettimeofday(&tv, NULL);
            elem->timestamp_logout = tv.tv_sec;
            salvaUserSuFile();
            return;
        }
    }
    liberaUser();
    fprintf(stderr, "errore: non è stato trovato l'utente passato alla funzione %s", user_offline->user_name);
}

/*
    cercaUserPerPorta è una funzione di utilità che data un porta 
    presa in ingresso alla funzione ritorna un puntatore alla UserEntry
    che è presente nel file di registro dove combaciano le porte. Oltre 
    al numero di porta da cercare, la funzione prende in ingresso un bool
    che indica se è necessario o meno caricare gli user da rubrica
*/
UserEntry* cercaUserPerPorta(int port, bool carica){
    UserEntry* elem;
    if(carica)
        caricaUserDaFile();
    for(elem = Server.user_registrati; elem!=NULL; elem = elem->next){
        if(elem->port == port)
            break;
    }
    return elem;
}

/*
    cercaUserPerPorta è una funzione di utilità che dato un username 
    presa in ingresso alla funzione ritorna un puntatore alla UserEntry
    che è presente nel file di registro dove combaciano gli username. Oltre 
    al numero di porta da cercare, la funzione prende in ingresso un bool
    che indica se è necessario o meno caricare gli user da rubrica
*/
UserEntry* cercaUserPerNome(char* username, bool carica){
    UserEntry* elem;
    if(carica)
        caricaUserDaFile();
    for(elem = Server.user_registrati; elem!=NULL; elem=elem->next){
        if(!strcmp(username, elem->user_name)){
            break;
        }
    }
    return elem;
}
/*
    hangMessaggio è la funzione che scrive il messaggio di chat per un utente
    nel suo file di messaggi in attesa, prede in ingresso lo user che ha inviato
    il messaggio (user_source) che si trova in buffer e lo user destinatario 
    (user_offline)
*/
void hangMessaggio(UserEntry *user_offline, UserEntry* user_source, char* buffer){
    FILE* fp;
    struct  timeval tv;
    char* msg;
    
    int str_len = strlen(user_offline->user_name)+1;
    char tmp[str_len];
    DEBUG_PRINT(("%s", user_offline->user_name));

    //genero il filepath per il file
    //di hanging dello user
    //formato del filepath:
    //user_offline/hanging/user_source.txt
    strcpy(tmp, user_offline->user_name);
    str_len += strlen(user_source->user_name)+1;
    char filepath[str_len+15];
    strcat(tmp,"/hanging/");
    strcat(tmp,user_source->user_name);
    strcpy(filepath,tmp);
    strcat(filepath, ".txt");
    DEBUG_PRINT(("%s", filepath));

    fp = fopen(filepath, "a");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", filepath));
    } 
    //se è stato possibile aprire correttamente il file in append con successo
    //scrivo il messaggio buffer su file con il timestamp dell'invio
    else {
        gettimeofday(&tv, NULL);
        msg = malloc(sizeof(buffer)+11*sizeof(int));
        sprintf(msg, "%ld %s", tv.tv_sec, buffer);
        fprintf(fp, "  %s", msg);
        free(msg);
    }
    fclose(fp);

}

/*
    scriviCronologiaDiChat è la funzione che scrive su file la cronologia
    dei messaggi di chat segnalando se il messaggio è stato ricevuto (* ) o
    ricevuto e letto (**). La funzione prende in ingresso il messaggio da scrivere
    su file, il sender, il receiver e il numero di * da inserire
*/
void scriviCronologiaDiChat(char* msg, char* username_dst, char* username_src, int count){
    FILE* fp;
    char* line = NULL;
    int len, read;
    DEBUG_PRINT(("%s, %s, %s ,%d", msg, username_dst, username_src, count));
    char filepath[strlen(username_dst)+strlen(username_src) + strlen("/chat_.txt")+1];

    //creo il filepath che presenta il seguente formato:
    //username_dst/chat_username_src.txt
    strcpy(filepath, username_dst);
    strcat(filepath,"/chat_");
    strcat(filepath,username_src);
    strcat(filepath, ".txt");
    DEBUG_PRINT(("%s", filepath));

    fp = fopen(filepath, "a");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", filepath));
    } else {

        //se il file è stato aperto in modalita append correttamente
        //scrivo su file il messaggio preceduto da count *
        if(count ==0)
            fprintf(fp, "  %s",msg);
        else if(count ==1)
            fprintf(fp, " *%s", msg);
        else
            fprintf(fp, "**%s", msg);
    }
    fclose(fp);
}

/*
    inviaMessaggioDiChat è la funzione che dato un messaggio contenuto in 
    buffer inviato dal device in ascolto su port_source, lo inoltra allo user
    identificato dal suo nome contenuto in user_dest
    la funzione ritorna:
        -1 in caso di errore inaspettato
        0 se user_dest è offline
        1 se il messaggio è stato recapitato correttamente
*/
int inviaMessaggioDiChat(char* user_dest, int port_sorce, char* buffer){
    int sd, ret;
    char* msg, tmp;
    char risposta_pending[USER_LEN];
    char operazione[OP_LEN];
    struct timeval tv;
    msg = buffer;
    
    UserEntry* elem_dest, *elem_src;
    UserPending* pending_src;
    /* Unresolved scoping issue
    elem_dest =cercaUserPerNome(user_dest, true);
    elem_src = cercaUserPerPorta(port_sorce, true);*/
    caricaUserDaFile();
    for(elem_dest = Server.user_registrati; elem_dest!=NULL; elem_dest=elem_dest->next){
        if(!strcmp(user_dest, elem_dest->user_name)){
            break;
        }
    }
    for(elem_src = Server.user_registrati; elem_src!=NULL; elem_src=elem_src->next){
        if(port_sorce== elem_src->port){
            break;
        }
    }

    //msg non contiene soltanto il messaggio di chat, ma anche lo user_dest e 
    //port_source quindi aggiorno il puntatore per escludere questi elementi
    //dal messaggio principale    
    msg += strlen(user_dest)*sizeof(char) + 6;
    DEBUG_PRINT(("%d %s %s", elem_dest->port, elem_src->user_name, msg));
    if(elem_dest==NULL){
        DEBUG_PRINT(("non esiste un utente sulla porta %d", elem_dest->port));
        return -1;
    }

    //se lo user_dest è offline, aggiorno i file di cronologia della chat e il
    //file di hanging
    if(elem_dest->timestamp_login < elem_dest->timestamp_logout){
        scriviCronologiaDiChat(msg, elem_dest->user_name, elem_src->user_name, 0);
        scriviCronologiaDiChat(msg, elem_src->user_name, elem_dest->user_name, 0);
        hangMessaggio(elem_dest, elem_src, msg);
        return 0;
    }

    //provo a collegarmi con user_dest per un massimo di tre volte
    while(elem_dest->tentativi_connessione>0){
        sd = creaConnessioneTCP(elem_dest->port);
        if(sd>=0){
            break;
        }
        elem_dest->tentativi_connessione--;    
    }

    //se non sono riuscito a collegarmi con lo user_dest e ho
    //esaurito i tentativi, ciò vuol dire che user_dest è offline
    //per cui aggiorno il suo timestamp di logout e file di cronologia
    //messaggi e di hanging
    if(elem_dest->tentativi_connessione==0){
        DEBUG_PRINT(("utente offline"));
        gettimeofday(&tv, NULL);
        elem_dest->timestamp_logout = tv.tv_sec;
        elem_dest->tentativi_connessione = 3;
        hangMessaggio(elem_dest, elem_src, msg);
        scriviCronologiaDiChat(msg, elem_dest->user_name, elem_src->user_name, 1);
        scriviCronologiaDiChat(msg, elem_src->user_name, elem_dest->user_name, 1);
        salvaUserSuFile();
        return 0;
    }

    //se user_dest è online inoltro il messaggio e mi metto in ascolto
    //per una risposta
    elem_dest->tentativi_connessione=3;
    ret = inviaMessaggioTCP(sd, "CHT", (void*)msg, strlen(msg));
    if(ret<0){
        perror("errore in fase di send del messaggio di chat");
        return -1;
    }
    
    riceviMessaggioTCP(sd, operazione, (void*)&tmp);
   
    //se user_dest risponde con OK! vuol dire che il messaggio è stato
    //ricevuto e letto
    if(!strcmp("OK!", operazione)){
        scriviCronologiaDiChat(msg, elem_dest->user_name, elem_src->user_name, 2);
        scriviCronologiaDiChat(msg, elem_src->user_name, elem_dest->user_name, 2);
        liberaUser();
        return 1;
    }

    //altrimenti il messaggio è stato ricevuto, ma non è stato possibile leggerlo
    else{
        scriviCronologiaDiChat(msg, elem_dest->user_name, elem_src->user_name, 1);
        scriviCronologiaDiChat(msg, elem_src->user_name, elem_dest->user_name, 1);
        liberaUser();
        return 0;
    }
}

/*
    contaMessaggi è una funzione di utilità che dato un filepath, 
    ritorna il numero di righe presenti nel file, -1 in caso di errore
    di apertura del file identificato da filepath
*/
int contaMessaggi(char* filepath){
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    int count = 0;
    fp = fopen(filepath, "r");
    
    //se l'operazione di apertura del file in lettura non va
    //a buon fine ritorno -1 al chiamante
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", filepath));
        return -1;
    } else {

        //altrimenti scorro le righe presenti nel file
        //e incremento un contatore che verrà poi 
        //ritornato al chiamante
        while ((read = getline(&line, &len, fp)) != -1){
            count++;
        }
    }
    return count;
}

/*
    inviaMessaggioDiHang è la funzione che dato uno user identificato
    dall'username che prende in ingresso invia al device su cui è loggato
    il nome degli utenti che gli hanno inviato messaggi mentre era offline,
    il numero dei messaggi per utente e il timestamp dell'ultimo ricevuto
    per utente

    ATTENZIONE il codice per questa funzione, con alcuni cambiamenti, è copiato da:
    https://stackoverflow.com/questions/1271064/how-do-i-loop-through-all-files-in-a-folder-using-c
*/
void inviaMessaggioDiHang(char* username){
    UserEntry* elem;
    struct dirent *dp;
    DIR *dfd;
    char dir[strlen(username)+strlen("/hanging")+1];
    int sd;

    elem = cercaUserPerNome(username, true);
    
    //creo il path per la dir
    strcpy(dir,username);
    strcat(dir,"/hanging");
    DEBUG_PRINT(("%s", dir));
    if ((dfd = opendir(dir)) == NULL){
        DEBUG_PRINT(("Can't open %s\n", dir));
        return;
    }
    //se è possibile aprire la cartella scorro
    //attravarso tutti gli elementi della dir
    char filename_qfd[BUFFER_LEN] ;
    while ((dp = readdir(dfd)) != NULL){
        struct stat stbuf;
        int count;
        char filepath[100];
        char msg[BUFFER_LEN];
        sprintf( filename_qfd , "%s/%s",dir,dp->d_name);

        //associo un elemento della a una stringa che lo identifica
        if( stat(filename_qfd,&stbuf ) == -1 ){
        printf("Unable to stat file: %s\n",filename_qfd);
        continue ;
        }

        //se è una dir passo all'iterazione successiva
        if (S_ISDIR(stbuf.st_mode)){
            continue;
        }

        //copio il path che identifica la cartella in filepath
        strcpy(filepath, dir);
        strcat(filepath, "/");
        strcat(filepath, dp->d_name);
        
        //conto il numero di messaggi contenuti dentro un file
        //identificato da filepath
        count = contaMessaggi(filepath);
        
        //genero il messaggio di hanging e lo invio al client
        sprintf(msg, "%s %d %s", dp->d_name, count, ctime(&stbuf.st_mtime));
        DEBUG_PRINT(("%s", msg));
        sd = creaConnessioneTCP(elem->port);
        inviaMessaggioTCP(sd, "HGR", (void*)msg, strlen(msg));
    }
    liberaUser();
}

/*
    aggiornaFileDiChat è la funzione che aggiorna i file di cronologia di chat
    dell'utente identificato dal nome username_src relativo all'utente
    identificato da username_dst
*/
void aggiornaFileDiChat(char* username_src, char* username_dst){
    char filepath[strlen(username_src)+strlen("/chat_.txt")+1];
    int sd, ch;
    FILE* fp;
    char* line = NULL;
    char tmp[strlen(username_dst)+1];
    size_t len = 0;
    ssize_t read;

    strcpy(tmp, username_dst);
    strcpy(filepath,username_src);
    strcat(filepath,"/chat_");
    strcat(filepath, tmp);
    strcat(filepath, ".txt");

    DEBUG_PRINT(("%s", filepath));
    fp = fopen(filepath, "r+");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", filepath));
        return;
    }

    //se il file viene aperto in r+ senza problemi scorro il file fino alla fine
    while ((ch = fgetc(fp)) != EOF)
    {
        //se mi trovo sulla fine di una riga devo eseguire ulteriori controlli
        if(ch =='\n'){
            //devo prima accertarmi che la riga su cui mi trovavo non fosse l'ultima
            if((ch = fgetc(fp)) == EOF){
                return;
            }

            //se la nuova riga non presenta i due ** , allora aggiorno la riga
            if((ch = fgetc(fp)) != '*'){
                fseek(fp, -1, SEEK_CUR);
                fputc('*',fp);
                fseek(fp, 0, SEEK_CUR);
            }
            if((ch = fgetc(fp)) != '*'){
                fseek(fp, -1, SEEK_CUR);
                fputc('*',fp);
                fseek(fp, 0, SEEK_CUR);
            }

        }
    }
    fclose(fp);

}
/*
    inviaMessaggioDiShow è la funzione che invia al richiedente identificato da
    username_src i messaggi non ancora letti inviatogli da username_dst
*/
void inviaMessaggioDiShow(char* username_src, char* username_dst){
    UserEntry* elem_src;
    UserEntry* elem_dst;
    struct dirent *dp;
    char filepath[strlen(username_src)+strlen("/hanging/")+1];
    int sd, count;
    long timestamp;
    FILE* fp;
    char* line= NULL;
    char counter[3];
    char tmp[strlen(username_dst)+1];
    size_t len = 0;
    ssize_t read;
    elem_src = cercaUserPerNome(username_src, true);

    //genero il filepath per il file
    strcpy(tmp, username_dst);
    strcpy(filepath,username_src);
    strcat(filepath,"/hanging/");
    strcat(filepath, tmp);
    strcat(filepath, ".txt");
    DEBUG_PRINT(("%s", filepath));

    //conto il numero di messaggi presenti nel file
    count = contaMessaggi(filepath);
    DEBUG_PRINT(("%d", count));
    fp = fopen(filepath, "r");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", filepath));
        return;
    } 
    else {
        //provo a collegarmi con l'utente che ha fatto la richiesta
        //e gli invio preventivamente il numero di messaggi non ancora letti
        sd = creaConnessioneTCP(elem_src->port);
        DEBUG_PRINT(("%d", sd));
        sprintf(counter, "%d", count);
        DEBUG_PRINT(("%s %d", counter, count));
        inviaMessaggioTCP(sd, "SWR", (void*)counter, strlen(counter));
 
        //scorro il file e invio al richiedente i messaggi non letti
        while ((read = getline(&line, &len, fp)) != -1){
            DEBUG_PRINT(("%s", line));
            inviaMessaggioTCP(sd, "SHL", (void*)line, strlen(line)+1);
            sleep(0.200);
        }

        //pulisco il file
        fp = fopen(filepath, "w");
        if (fp == NULL) {
            DEBUG_PRINT(("Impossibile aprire file %s", filepath));
            return;
        } else {
            fprintf(fp, "%s", "");        
        }
    }
    DEBUG_PRINT(("!!%s %s!!", username_src, username_dst));
    //aggiorno i file di chat in quanto i messaggi pendenti sono stati letti
    aggiornaFileDiChat(username_src,username_dst);
    aggiornaFileDiChat(username_dst,username_src);
    
    //provo a contattare l'utente che aveva inviato i messaggi che sono stati appena letti
    elem_dst = cercaUserPerNome(username_dst, false);
    sd = creaConnessioneTCP(elem_dst->port);
    if(sd<0){
        //se l'utente non è raggiungibile, inserico una nuova entrata nel file di 
        //user_pending
        DEBUG_PRINT(("setta elemento"));
        UserPending* this_user_pending = malloc(sizeof(UserPending));
        strcpy(this_user_pending->username_sender, elem_dst->user_name);
        strcpy(this_user_pending->username_receiver, elem_src->user_name);
        this_user_pending->port_sender = elem_dst->port;
        this_user_pending->next=NULL;
        inserisciUserPending(this_user_pending, &Server.user_pending);
        salvaUserPendingSuFile(Server.user_pending);
        DEBUG_PRINT(("%s %s", Server.user_pending->username_sender, Server.user_pending->username_receiver));
    }
    else{
        inviaMessaggioTCP(sd, "SRC", "", 0);
    }

    if(line)
        free(line);
    
    liberaUser();

}

/*
    inviaCronologiaDiChat è la funzione che a seguito di una creazione di chat
    invia al richiedente la cronologia della chat allo stesso.
*/
void inviaCronologiaDiChat(char* username_src, char* username_dst){
    char filepath[strlen(username_src)+strlen("/chat_.txt")+1];
    int sd, ch, count;
    FILE* fp;
    char* line = NULL;
    char counter[3];
    char tmp[strlen(username_dst)+1];
    size_t len = 0;
    ssize_t read;
    UserEntry* elem_src = cercaUserPerNome(username_src, true);
    
    //genero il filepath per il file di cronologia di chat
    strcpy(tmp, username_dst);
    strcpy(filepath,username_src);
    strcat(filepath,"/chat_");
    strcat(filepath, tmp);
    strcat(filepath, ".txt");

    DEBUG_PRINT(("%s", filepath));

    //conto il numero dei messaggi presenti nel file
    count = contaMessaggi(filepath);
    fp = fopen(filepath, "r");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", filepath));
        return;
    } else {
        //mi collego col richiedente e gli invio preventivamente il numero dei
        //messaggi presenti in cronologia
        sd = creaConnessioneTCP(elem_src->port);
        DEBUG_PRINT(("%d", sd));
        sprintf(counter, "%d", count);
        DEBUG_PRINT(("%s %d", counter, count));
        inviaMessaggioTCP(sd, "LOG", (void*)counter, strlen(counter));

        //scorro il file e invio i messaggi al richiedente
        while ((read = getline(&line, &len, fp)) != -1){
            if(!strcmp("\n", line))
                continue;
            char* msg = malloc(strlen(line));
            strcpy(msg, line);
            DEBUG_PRINT(("%s", msg));
            inviaMessaggioTCP(sd, "LGM", (void*)msg, strlen(msg));
            free(msg);
            sleep(0.200);
        }
    }
    liberaUser();
}

/*
    gesticiInputTCP è la funzione principale che gestisce gli input in arrivo 
    tramite connessione TCP, la funzione prende in ingresso il socket descriptor
    che identifica la connessione
    A seguito i codici di operazione, il loro significato e il formato dei messaggi in ingresso
    SGP --> richiesta di signup, formato: SGP username_src password_src port_src
    IN- --> richiesta di signin, fomato: IN- server_port username_src password_src
    OUT --> richiesta di signout, formato: OUT port_src
    CHT --> richiesta di inoltrare un messaggio di chat, formato: CHT messaggio_di_chat_con_header
    HNG --> richiesta di hanging, formato HNG username_src
    SHW --> richiesta di show, formato: SHW username_src username_dst
    CHR --> richiesta di creazione chat, formato: CHR username_src username_dst
    ONR --> richiesta di mostrare gli user_online, formato: ONR port_src
    UPR --> richiesta per la porta di ascolto di uno specifico utente, formato: UPR username_target

*/

void gestisciInputTCP(int sd){
    char operazione[OP_LEN];
    char username[USER_LEN];
    char username_src[USER_LEN];
    char password[PASSWORD_LEN];
    int port_server, port;
    char* msg;
    char* tmp;
    char buffer[BUFFER_LEN];
    int ret, ctrl;
    UserEntry* elem;

    //ricevo il messaggio in ingresso
    memset(operazione, '\0', OP_LEN);
    ret = riceviMessaggioTCP(sd, operazione, (void*)&msg);
    if(ret<0){
            perror("errore in fase di ricezione del messaggio:");
            exit(-1);
    }

    //se l'operazione di ricezione è andata a buon fine passo 
    //a controllare il codice dell'operazione
    if(!strcmp("SGP",operazione)){
        DEBUG_PRINT(("Operazione di signup"));
        sscanf(msg, "%s %s %d", username, password, &port);
        
        //controllo se esiste già un utente identificato da username
        if(controllaCrendenzialiSignup(username, password, port)){
            
            //se non esiste provo a creare una nuova cartella 
            //che prende il nome dello user che ha fatto la richiesta di signup
            if(!mkdir(username, 0777)){
                DEBUG_PRINT(("cartella creata %s", username));
            }
            else {
                DEBUG_PRINT(("errore nella creazione della cartella %s", username));
                ret = inviaMessaggioTCP(sd, "ERR", "", 0);
                return;
            }

            //se l'operazione è andata a buon fine lo segnalo all'utente
            ret = inviaMessaggioTCP(sd, "OK!", "", 0);
        }
        else{
            //se l'operazione è fallita avviso l'utente che esiste già
            //qualcuno con quel username
            ret = inviaMessaggioTCP(sd,"KNW", "", 0);
        }
    }
    else if(!strcmp("IN-", operazione)){
        DEBUG_PRINT(("Operazione di IN"));
        sscanf(msg, "%d %s %s %d", &port_server, username, password, &port);

        //controllo se l'utente ha inserito la server_port corretta
        //in caso contrario segnalo all'utente l'errore nell'inserire la
        //porta (codice: EPT)
        if(port_server!=Server.port){
            DEBUG_PRINT(("porta di server %d errata", port));
            inviaMessaggioTCP(sd, "EPT", "", 0);
            return;
        }

        //controllo se l'utente che ha fatto la richiesta di 
        //signin esiste già e ha inserito correttamente la password
        //se il controllo è positivo segnalo all'utente il successo
        //dell'operazione 
        if(controllaCrendenzialiIn(username, password, port)){
            DEBUG_PRINT(("login effettuato per %s", username));
            inviaMessaggioTCP(sd, "OK!", "", 0);
        }

        //altrimenti segnalo all'utente che vi è stato un problema
        //nell'autenticazione (codice: EUP)
        else{
            inviaMessaggioTCP(sd, "EUP", "", 0);
            DEBUG_PRINT(("login fallito per %s", username));
        }
    }
    else if(!strcmp("OUT", operazione)){
        sscanf(msg, "%d", &port);
    
        //controllo se è possibile eseguire l'operazione di logout con 
        //successo, se sì avviso l'utento che l'operazione è andata 
        //a buon fine
        if(logutUser(port)){
            inviaMessaggioTCP(sd, "OK!", "", 0);
            DEBUG_PRINT(("logout eseguito per device in ascolto su %d", port));
        }

        //altrimenti segnalo un errore all'utente
        else{
            inviaMessaggioTCP(sd, "ERR", "", 0);
            DEBUG_PRINT(("logout fallito per device in ascolto su %d", port));
        }
    }
    else if(!strcmp("CHT", operazione)){
        DEBUG_PRINT(("messaggio di chat in ingresso"));
        sscanf(msg, "%s %d", username, &port);

        //inoltro il messaggio di chat al destinatario e controllo se 
        //l'operazione è andata a buon fine ctrl == 1, se vi è stato un 
        //errore non previsto ctrl == -1 o se lo user dest è offline
        ctrl = inviaMessaggioDiChat(username, port, msg);
        //se l'operazione è andata a buon fine avviso l'utente
        if(ctrl==1){
            inviaMessaggioTCP(sd, "OK!", "", 0);
        }

        //se l'operazione è fallita segnalo l'errore all'utente
        else if(ctrl ==-1){
            inviaMessaggioTCP(sd, "ERR", "", 0);
        }

        //se lo user_dst era offline avviso l'utente_src che il messaggio è
        //stato recapitato, ma non è stato ricevuto (codice: OFF)
        else{
            inviaMessaggioTCP(sd, "OFF", "", 0);
        }
    }
    else if(!strcmp("HNG", operazione)){
        DEBUG_PRINT(("richiesta di haninging"));
        sscanf(msg, "%s", username_src);
        inviaMessaggioDiHang(username_src);

    }
    else if(!strcmp("SHW", operazione)){
        DEBUG_PRINT(("richiesta di show"));
        sscanf(msg, "%s %s", username_src, username);
        DEBUG_PRINT(("%s %s", username_src, username));
        inviaMessaggioDiShow(username_src, username);
    }
    else if(!strcmp("CHR", operazione)){
        DEBUG_PRINT(("richiesta di chat"));
        UserPending* elem;
        caricaUserPendingDaFile();
        sscanf(msg, "%s %s", username_src, username);

        //invio al richiedente la cronologia di chat
        inviaCronologiaDiChat(username_src, username);

        //eseguo un controllo per vedere se l'utente richiedente aveva delle 
        //notifiche non lette
        for(elem = Server.user_pending; elem!=NULL; elem=elem->next){
            if(!strcmp(elem->username_receiver,username) && !strcmp(elem->username_sender, username_src)){
                break;
            }
        }
        DEBUG_PRINT(("%s %s", elem->username_sender, elem->username_receiver));
        
        //se sono presenti delle notifiche non lette, invio un messaggio
        //all'utente richiedente
        if(elem!=NULL){
            sd=creaConnessioneTCP(elem->port_sender);
            inviaMessaggioTCP(sd, "PMS", (void*)username, strlen(username));
        }
        
        //rimuovo la notifica e aggiorno il file
        eliminaUserPending(elem, &Server.user_pending);
        salvaUserPendingSuFile(Server.user_pending);
    }
    else if(!strcmp("ONR", operazione)){
        sscanf(msg, "%d", &port);
        mostraUserOnline(port);
    }
    else if(!strcmp("UPR", operazione)){
        //dato uno username in ingresso invio al richiedente la
        //porta su cui il device, a cui l'utente è collegato, è in ascolto
        sscanf(msg, "%s", username);
        DEBUG_PRINT(("%s", username));
        elem = cercaUserPerNome(username, true);
        sprintf(buffer, "%d", elem->port);
        DEBUG_PRINT(("%s", buffer));
        inviaMessaggioTCP(sd, "UPA", (void*)buffer, strlen(buffer));
        liberaUser();
    }

    if (msg)
        free(msg);
    close(sd);
    FD_CLR(sd, &fds.master);
}

/*
    controlloInput è la funzione principale che gestisce lo IOMultiplexing,
    sfrutta la struttura dati file_descriptor_set fds per la gestione dei
    file descriptor
*/
void controllaInput(){
    int ret, newfd,  listener, i;
    socklen_t addrlen;
    struct sockaddr_in my_addr, cl_addr;
    char buffer[4096];
    pid_t pid;
    struct timeval tv;

    //viene creato il socket, sttatto l'indirizzo del device 
    //e viene eseguita la bind
    listener = socket(AF_INET, SOCK_STREAM, 0);
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(Server.port);
    inet_pton(AF_INET, "127.0.0.1", &my_addr.sin_addr);

    ret = bind(listener, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if (ret < 0) {
        perror("Bind listener non riuscita: ");
        exit(0);
    }

    //se la bind viene eseguita con successo, viene creata la coda di ingresso,
    //vengono resettati i file descriptor e successivamente inserito il fd per
    //il listener
    listen(listener, 10);
    DEBUG_PRINT(("in ascolto sulla porta %d", Server.port));

    FD_ZERO(&(fds.master));
    FD_ZERO(&(fds.read_fds));

    FD_SET(listener, &(fds.master));
    fds.fdmax = listener;
    while (1) {
        //viene effettuata la pulizia del buffer e viene chiamata la select
        memset(buffer, '\0', 4096);
        fds.read_fds = fds.master;
        i = select(fds.fdmax + 1, &(fds.read_fds), NULL, NULL, NULL);
        if (i < 0) {
            perror("select: ");
            exit(1);
        }
        if (i >= 0) {
            
            //scorro tutti i fds presenti
            for (i = 0; i <= fds.fdmax+1; i++) {
                if (FD_ISSET(i, &(fds.read_fds))) {
                    DEBUG_PRINT(("%d is set\n", i));

                    //se i==listener vuol dire che è arrivata una richiesta 
                    //di connessione TCP e, dopo essere stata accettata
                    //viene aggiunto il relativo fd al fds
                    if (i == listener) { // 
                        addrlen = sizeof(cl_addr);
                        newfd = accept(listener, (struct sockaddr *)&cl_addr, &addrlen);
                        FD_SET(newfd, & fds.master);
                        if (newfd > fds.fdmax)
                            fds.fdmax = newfd;
                    }

                    //altrimenti vuol dire che è stato rilevato un messaggio TCP
                    //per cui viene creato un figlio per gestire la connessione
                    else {
                        pid = fork();
                        if (pid == -1) {
                            perror("Errore durante la fork: ");
                            return;
                        }
                        if (pid == 0) { // figlio
                            close(listener);
                            printf("handleTCP\n");
                            gestisciInputTCP(i);
                            exit(0);
                        }
                        close(i);
                        FD_CLR(i, &(fds.master));
                    }
                }
            }
        }
    }
    close(listener);
}

int main(int argc, char* argv[]){
    pid_t pid;
    int listener;
    if(argc != 1){
        Server.port = atoi(argv[1]);
    }
    DEBUG_PRINT(("%d\n", Server.port));
    printf("\n\t********* SERVER : %d *********\n", Server.port);
    printf("Digita un comando:\n");
    printf("1) help --> mostra i dettagli dei comandi\n");
    printf("2) list --> mostra un elenco degli utenti connessi\n");
    printf("3) esc --> chiude il server\n");
    pid = fork();
    if(pid == 0){
        controllaInput();
    }
    else{
        while(1){
            char tmp[20];
            scanf("%s", tmp);
            if(!strcmp("esc", tmp)){
                kill(pid, SIGUSR1);
                printf("Arrivederci\n");
                exit(0);
            }
            else if(!strcmp("help", tmp)) {
                printf("help: Mostra una breve descrizione dei comandi.\n");
                printf("list: Mostra l’elenco degli utenti connessi alla rete, ");
                printf("indicando username, timestamp di connessione e numero di ");
                printf("porta nel formato \"username*timestamp*porta\"\n");
                printf("esc: Termina il server");
            } else if(!strcmp("list", tmp)) {
               mostraUserOnline(0);
            } else {
                printf("comando invalido\n");
            }
        }
    }
}