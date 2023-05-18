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
#include"utility/TCP.h"
#include"utility/UserContact.h"
#define DEBUG_ON

#ifdef DEBUG_ON
    #define DEBUG_PRINT(x) printf("[DEBUG]: "); printf x; printf("\n");
#endif

#define STDIN 0
#define OP_LEN 4
#define USER_LEN 30
#define PASSWORD_LEN 30
#define BUFFER_LEN 1024

/*
    UserContact è una struttura dati che serve per tenere traccia
    degli utenti con cui il device sta chattando

typedef struct UserContact_s UserContact;
struct UserContact_s {
    char username[USER_LEN];    //stringa per il nome dell'utente
    int port;                   //porta su cui è possibile contattare l'utente
    UserContact* next;          //puntatore al prossimo utente in chat
};
*/
/*
    Device_s è la struttura dati che serve per tenere traccia
    di tutte le informazioni relative al device attuale
*/

struct Device_s {
    bool logged;                //l'utente su questo device ha effettuato il login
    bool chatting;              //l'utente è all'interno di una chat
    char username[USER_LEN];    //stringa per il nome dell'utente su questo device
    char password[PASSWORD_LEN];//stringa per la password dell'utento su questo device
    int port;                   //porta su cui è in ascolto il device
    int server_port;            //la porta su cui è raggiungibile il server (default: 4242)
    UserContact* lista_contatti;// lista dei contatti nella rubrica dell'utente loggato
    UserContact* lista_contatti_chat; //lista dei contatti con cui l'utente sta chattando
} Device = {
    false,  //logged
    false,  //chatting
    "",     //username
    "",     //password
    -1,     //port
    4242,   //server_port
    NULL,   //lista_contatti
    NULL    //lista_contatti_chat
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
    caricaUserDaRubrica si occupa di leggere da file gli utenti presenti
    nella rubrica di uno specifico user e di caricare i dati nella struttura
    UserContact
*/
void caricaUserDaRubrica(){
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    char tmp[strlen(Device.username)+1];

    //genero il filepath per il file
    strcpy(tmp, Device.username);
    char filepath[strlen(Device.username)+strlen("/rubrica.txt")+1];
    strcpy(filepath,strcat(tmp,"/rubrica.txt"));

    //apro il file in modalità lettura e lo scorro riga per riga
    fp = fopen(filepath, "r");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", filepath));
    } else {
        while ((read = getline(&line, &len, fp)) != -1) {
            //creo una istanza di UserContact in cui inserire i dati 
            //letti da file
            UserContact* this_user = malloc(sizeof(UserContact));
            DEBUG_PRINT(("%s", line));
            if (!sscanf(line, "%s %d",
                        this_user->username,
                        &this_user->port) 
            ){
                DEBUG_PRINT(("errore nel parsing di un record nel file %s", filepath));
            } else {
                //inserisco il nuovo elemento in lista_contatti
                this_user->next = NULL;
                inserisciUser(this_user, &Device.lista_contatti);
            }
        }
    fclose(fp);
    if (line)
        free(line);
    }
}


/*
    signup si occupa di inviare al server la richiesta di signup,
    prende in ingresso lo username e la password dello user che 
    effettua la richiesta di registrazione
*/
void signup(char* username, char* password){
    int sd;
    char msg[BUFFER_LEN];
    char operazione[OP_LEN];

    sd = creaConnessioneTCP(Device.server_port);
    DEBUG_PRINT(("creato nuovo sd> %d", sd));
    if(sd < 0){
        perror("errore nella creazione di sd: ");
        return;
    }
    //se l'operazione di creazione di connessione è andata a buon fine
    //creo il messaggio contenete le informazioni per le operazione di 
    //signup e lo invio (codice: SGP)
    sprintf(msg, "%s %s %d", username, password, Device.port);
    DEBUG_PRINT(("%s\n", msg));
    if(!inviaMessaggioTCP(sd, "SGP", (void*)msg, strlen(msg))){
        perror("errore in fase di invio del messaggio:");
        exit(-1);
    }
    //mi metto in attesa di ricevere una risposta dal server e controllo
    //se l'operazione è andata a buon fine (codice: OK!) oppure no
    //(codice: ERR)
    if(!riceviMessaggioTCP(sd, operazione, (void*)&msg)){
        perror("errore in fase di ricezione del messaggio:");
        exit(-1);
    }
    if(!strcmp("OK!", operazione)){
        printf("signup avvenuto con successo, benvenuto %s\n", username);
    }
    else{
        printf("signup fallita\n");
    }
    

}

/*
    signin si occupa di inviare al server la richiesta di login,
    prende in ingresso lo username, la password dello user e
    la porta del sever
*/
void signin(char* username, char* password, int port){
    int sd;
    char msg[BUFFER_LEN];
    char operazione[OP_LEN];

    sd = creaConnessioneTCP(Device.server_port);
    DEBUG_PRINT(("creato nuovo sd> %d", sd));
    if(sd < 0){
        perror("errore nella creazione di sd: ");
        return;
    }
    //se l'operazione di creazione di connessione è andata a buon fine
    //creo il messaggio contenete le informazioni per le operazione di 
    //login e lo invio (codice: IN-)
    sprintf(msg, "%d %s %s %d", port, username, password, Device.port);
    DEBUG_PRINT(("%s", msg));
    if(!inviaMessaggioTCP(sd, "IN-", (void*)msg, strlen(msg))){
        perror("errore in fase di invio del messaggio:");
        exit(-1);
    }
    //mi metto in attesa di ricevere una risposta dal server e controllo
    //se l'operazione è andata a buon fine (codice: OK!), se è stata
    //inserita la porta scorretta (codice: EPT) o se username o password 
    //sono scorretti (codice: EUP)
    if(!riceviMessaggioTCP(sd, operazione, (void*)&msg)){
        perror("errore in fase di ricezione del messaggio:");
        exit(-1);
    }
    //se l'operazione ha successo carico sulla struttura UserContact
    //gli utenti presenti in rubrica e setto la variabile booleana 
    //logged di Device 
    if(!strcmp("OK!", operazione)){
        printf("signin avvenuto con successo, benvenuto %s\n", username);
        Device.logged = true;
        caricaUserDaRubrica();
    }
    else if (!strcmp("EPT", operazione)){
        printf("signin fallita, inserito numero di porta errato %d\n", port);
    }
    else if(!strcmp("EUP", operazione)){
        printf("signin fallita, inseriti user o password sbagliati %d\n", port);
    }
    
}

/*
    signout si occupa di inviare al server la notifica che il device
    desidera disconnettersi, prende in ingresso la porta del device che
    fa la richiesta
*/
void signout(int port){
    int sd;
    char msg[BUFFER_LEN];
    char operazione[OP_LEN];
    sprintf(msg, "%d",port);
    sd = creaConnessioneTCP(Device.server_port);
    if(sd < 0){
        perror("errore in fase di creazione della connessione:");
        exit(-1);
    }
    //se l'operazione di creazione di connessione è andata a buon fine
    //creo il messaggio contenete le informazioni per le operazione di 
    //logout e lo invio (codice: OUT)
    DEBUG_PRINT(("creato nuovo sd> %d", sd));
    if(!inviaMessaggioTCP(sd, "OUT", (void*)msg, strlen(msg))){
        perror("errore in fase di invio del messaggio:");
        exit(-1);
    }

    //mi metto in attesa di ricevere una risposta dal server e controllo
    //se l'operazione è andata a buon fine (codice: OK!), oppure no
    //(codice: ERR)
    if(!riceviMessaggioTCP(sd, operazione, (void*)&msg)){
        perror("errore in fase di ricezione del messaggio:");
        exit(-1);
    }
    if(!strcmp("OK!", operazione)){
        printf("arrivederci\n");
        Device.logged = false;
        exit(0);
    }
    else if (!strcmp("ERR", operazione)){
        printf("qualcosa è andato storto nella signout\n");
    }
}

/*
    controllaListaUtentiChat è una funzione di utilità che
    controlla se il contatto passato in ingresso è già presente nella 
    lista dei contatti in chat
*/
bool controllaListaUtentiChat(UserContact* elem){
    UserContact* ctr;
    for(ctr = Device.lista_contatti_chat; ctr != NULL; ctr = ctr->next){
        if(!strcmp(elem->username, ctr->username)){
            return true;
        }
    }
    return false;
}

/*
    controllaRubrica è una funzione di utilità che ricerca
    nella rubrica dell'utente che invoca la funzione, un utente
    con l'username passato in ingresso. La funzione ritorna un
    puntatore a UserContact contenente le informazioni dell'utente
    ricercato se presente in rubrica, NULL altrimenti
*/
UserContact* controllaRubrica(char* username){
    UserContact* elem = NULL;
    for(elem = Device.lista_contatti; elem!=NULL; elem = elem->next){
        if(!strcmp(username, elem->username)){
            if(controllaListaUtentiChat(elem)){
                return elem;
            }
            UserContact* ret = malloc(sizeof(UserContact));
            strcpy(ret->username, elem->username);
            ret->port = elem->port;
            ret->next = NULL;
            return ret;
        }
    }
    return NULL;
}

/*
    inviaMessaggioDiChat è la funzione che invia al server un messaggio di chat
    che dovrà essere inotrato all'utente che è in ascolto su port_dest, il device
    invia al server, oltre al messaggio di chat anche la porta su cui si trova in
    ascolto così che il server saprà dove inviare la risposta per verificare 
    se l'operazione è andata a buon fine
*/
void inviaMessaggioDiChat(char *user_dest, int port_source, char* buffer){
    int sd;
    char operazione[4];
    char *tmp;
    char* msg;
    sd = creaConnessioneTCP(Device.server_port);
    if(sd < 0){
        perror("errore in fase di creazione di connessione: ");
        exit(-1);
    }
    DEBUG_PRINT(("creato nuovo sd> %d", sd));

    //se la creazione di connessione è andata a buon fine
    //alloco memoria per inviare un messaggio al server
    //ed invio il messaggio
    msg = malloc(strlen(buffer)+strlen(user_dest)+10);
    sprintf(msg, "%s %d %s", user_dest, port_source, buffer);
    if(!inviaMessaggioTCP(sd, "CHT", (void*)msg, strlen(msg))){
        perror("errore in fase di invio del messaggio:");
        exit(-1);
    }

    //se l'invio del messaggio è andato a buon fine, libero
    //la memoria allocata e mi metto in attesa per una risposta
    //se vie è stato un errore nell'inoltrare il messaggio (codice: ERR)
    //o se l'utente è offline (codice: OFF) lo segnalo all'utente
    //altrimenti non stampo nulla a video
    free(msg);
    if(!riceviMessaggioTCP(sd, operazione, (void*)&tmp)){
            perror("errore in fase di ricezione del messaggio:");
            exit(-1);
    }
    if(!strcmp("OK!", operazione)){
        DEBUG_PRINT(("messaggio recapitato\n"));
    }
    else if (!strcmp("ERR", operazione)){
        printf("qualcosa è andato storto nell'invio del messaggio\n");
    }
    else if(!strcmp("OFF", operazione)){
        printf("utente contattato offline\n");
    }

}

/*
    messaggioDiHang è la funzione che invia al server la richiesta
    di hanging (codice: HNG), la funzione invia al server lo username
    dell'utente attivo sul device
*/
void messaggioDiHang(){
    int sd;
    sd = creaConnessioneTCP(Device.server_port);
    if(sd < 0){
        perror("errore in fase di creazione di connessione: ");
        exit(-1);
    }
    
    DEBUG_PRINT(("creato nuovo sd> %d", sd));
    if(!inviaMessaggioTCP(sd, "HNG", (void*)Device.username, strlen(Device.username))){
        perror("errore in fase di invio del messaggio:");
        exit(-1);
    }
}

/*
    inviaRichiestaDiShow è la funzione che invia al server la richiesta di 
    show (codice: SHW), prende in ingresso lo username dell'utente oggetto
    della richiesta
*/
void inviaRichiestaDiShow(char* username){
    int sd;
    char msg[100];
    sd = creaConnessioneTCP(Device.server_port);
    if(sd < 0){
        perror("errore in fase di creazione di connessione: ");
        exit(-1);
    }
    sprintf(msg, "%s %s", Device.username, username);
    DEBUG_PRINT(("%s", msg));
    if(!inviaMessaggioTCP(sd, "SHW", (void*)msg, strlen(msg))){
        perror("errore in fase di invio del messaggio:");
        exit(-1);
    }
}

void chiudiChat(){
    Device.chatting = false;
}

/*
    inviaRichestaUserOnline è la funzione permette di richiedere al server
    la lista degli utenti attualmente online, prende in ingresso la porta
    su cui il device è in ascolto
*/
void inviaRichiestaUserOnline(int port){
    int sd;
    char msg[5];
    sprintf(msg, "%d", port);
    sd = creaConnessioneTCP(Device.server_port);
    inviaMessaggioTCP(sd, "ONR", (void*)msg, strlen(msg));
}


/*
    richiediPorta è la funzione che invia al server una richiesta per 
    sapere quale è la porta su cui il contatto passato in ingresso è in ascolto,
    la funzione ritorna true se ha successo, false altrimenti
*/
int richiediPorta(UserContact* elem){
    int sd, port;
    char operazione[OP_LEN];
    char* buffer;

    sd = creaConnessioneTCP(Device.server_port);
    if(sd<0){
        perror("Errore, impossibile collegarsi al server");
        exit(-1);
    }
    
    //viene inviata la richiesta al server, dopodiché mi metto in attesa
    //per ricevere il numero di porta dello user richiesto
    inviaMessaggioTCP(sd, "UPR", (void*)elem->username, strlen(elem->username));
    riceviMessaggioTCP(sd, operazione, (void*)&buffer);
    //if(!strcmp("UPA", operazione)){
        sscanf(buffer, "%d", &port);
        DEBUG_PRINT(("ricevuti %s %d", elem->username, port));
        return port;
    //}
    return 0;
}

/*
    trasferisciFile è la fuzione che invia un file a un'altro user,
    il file è identificato dalla stringa file_name
*/
void trasferisciFile(char* file_name){
    int sd, ret, len = 0;
    UserContact* elem;
    FILE* fp;
    char buffer[BUFFER_LEN];
    char operazione[OP_LEN];
    long dimensione, inviati;
    pid_t pid;
    memset(operazione, '\0', OP_LEN);
    if(Device.lista_contatti_chat==NULL){
        perror("Bisogna avere almeno una chat attiva per inviare un file\n");
        return;
    }
    
    //invio il file a tutti i contatti attualmente in chat
    for(elem=Device.lista_contatti_chat; elem!=NULL; elem=elem->next){
        //se ancora non conosco la porta degli utenti con cui sto chattando,
        //faccio la richiesta al server
        elem->port = richiediPorta(elem);
        
        //creo la connessione con il device_dst e invio 
        //il nome del file
        DEBUG_PRINT(("%d" ,elem->port));
        sd = creaConnessioneTCP(elem->port);
        if (sd == -1) {
            printf("impossibile trasferire file a %s\n", elem->username);
            return;
        }
        DEBUG_PRINT(("%s %d", file_name, strlen(file_name)));
        inviaMessaggioTCP(sd, "SHR", (void*)file_name, strlen(file_name)+1);
        
        //apro il file in lettura binaria
        fp = fopen(file_name, "rb");
        if (fp == NULL){
            perror("impossibile aprire il file: ");
            exit(0);
        }

        //calcolo della dimensione del file da condividere
        fseek(fp, 0, SEEK_END);
        dimensione = ftell(fp);
        //ripristino del puntatore all'inizio del file
        fseek(fp, 0, SEEK_SET);
        memset(buffer, '\0', sizeof(buffer));
        
        inviati = 0;
        //leggo da file e inserisco in buffer dopo di che invio il suo contenuto
        while((len = fread((void*)buffer, 1, sizeof(buffer), fp)) > 0) {
            DEBUG_PRINT(("%s %d", buffer, len));
            inviaMessaggioTCP(sd, "FLT", (void*)buffer, len);
            //pulisco il buffer per evitare problemi
            memset(buffer, '\0', sizeof(buffer));
            inviati+=len;
        }
        if (inviati != dimensione)
            perror("il file non è stato inviato correttamente: ");
        DEBUG_PRINT(("trasferimento finito"));
        //viene inviato il messaggio di fine trasferimento
        inviaMessaggioTCP(sd, "EOT", "", 0);
        DEBUG_PRINT(("inviato EOT"));
        close(sd);
        fclose(fp);

        //non è detto che al prossimo trasferimento file lo user
        //destinatario sia ancora sulla stessa porta per cui
        //resetto la porta
      
        elem->port = 0;
           
    }


}

/*
    controlloInputTastiera è la funzione principale di gestione del STDIN
    prende in ingresso la stringa digitata sul terminale e in base al 
    comando principale gestisce le azioni da eseguire
*/
void controlloInputTastiera(char*  buffer){
    char tmp[strlen(buffer)+1];
    char username[USER_LEN];
    int port, sd;
    UserContact* elem = NULL;
    char file_name[60];
    //leggo il primo elemento della stringa ergo il comando principale
    sscanf(buffer, "%s", tmp);
    
    //in base allo stata in cui si trova il device, lo user ha 
    //a disposizione un diverso set di comandi:
    //Device non loggato --> signup username password
    //                   --> in server_port username password
    //
    //Device loggato ma non in chat --> hanging
    //                              --> show username
    //                              --> chat username
    //                              --> share filename
    //                              --> out
    //Device loggato e in chat  --> [stringa contenete messaggio di chat]
    //                          --> \q + INVIO
    //                          --> \u + INVIO
    //                          --> \a + INVIO
    
    if(!Device.logged){
        if(!strcmp("signup", tmp)){
            DEBUG_PRINT(("signup"));
            //se l'operazione di lettura va a buon fine posso invocare la funzione di signup
            //altrimenti notifico l'errore all'utente
            if (sscanf(buffer, "%s %s %s", tmp, Device.username, Device.password) == 3) {
                signup(Device.username, Device.password);
            } else {
                printf("formato non valido per il comando signup username password \n digitati: %s %s %s \n", tmp, Device.username, Device.password);
            }
        }
        else if(!strcmp("in", tmp)){
            DEBUG_PRINT(("in"));
            //se l'operazione di lettura va a buon fine posso invocare la funzione di signin
            //altrimenti notifico l'errore all'utente
            if (sscanf(buffer, "%s %d %s %s", tmp, &port, Device.username, Device.password) == 4) {
                signin(Device.username, Device.password, port);
            } else {
                printf("formato non valido per il comando di signin server_port username password \n");
                printf("digitati: %s %d %s %s ricontrollare se i dati inseriti sono corretti\n", tmp, port, Device.username, Device.password);
            }
            
        }
    }
    else if(Device.logged && !Device.chatting){
        if(!strcmp("hanging", tmp)){
            DEBUG_PRINT(("hanging"));
            messaggioDiHang();
        }
        else if(!strcmp("show", tmp)){
            DEBUG_PRINT(("show"));
            if((sscanf(buffer, "%s %s", tmp, username)==2)){
                inviaRichiestaDiShow(username);
            }
            else{
                printf("formato non valido per il comando show username \n digitati: %s %s \n", tmp, username);
            }
        }
        else if(!strcmp("chat", tmp)){
            DEBUG_PRINT(("chat"));
            if(sscanf(buffer, "%s %s", tmp, username)!=2){
                printf("formato non valido per il comando chat username \n digitati: %s %s \n", tmp, username);
                return;
            }
            //cerco lo user desiderato nella rubrica dell'utente che fa la richiesta 
            //e se questo viene trovato procedo a inserire lo user nella lista dei contatti
            //in chat, altrimenti segnalo l'errore all'utente
            elem = controllaRubrica(username);
            if(elem != NULL){
                Device.chatting = true;
                if(!controllaListaUtentiChat(elem)) 
                    inserisciUser(elem, &Device.lista_contatti_chat);
                
                printf("in chat con %s\n", Device.lista_contatti_chat->username);
                sprintf(tmp, "%s %s", Device.username, username);
                sd = creaConnessioneTCP(Device.server_port);
                inviaMessaggioTCP(sd, "CHR", (void*)tmp, strlen(tmp));
                elem = Device.lista_contatti_chat;
                for(elem; elem!=NULL; elem = elem->next){
                    DEBUG_PRINT(("!!!!%s!!!!", elem->username));
                }

            }
            else{
                DEBUG_PRINT(("user %s non presente in rubrica", username));
            }
        }
        
        //se il comando è out, eseguo la funzione di signout ed termino
        //il programma 
        else if(!strcmp("out", tmp) || !strcmp("esc", tmp)){
            signout(Device.port);
            exit(0);
        }
        else if(!strcmp("share", tmp)){
            DEBUG_PRINT(("share"));
            sscanf(buffer, "%s %s", tmp, file_name);
            trasferisciFile(file_name);
        }
    }

    else if(Device.logged && Device.chatting){
        char da_user_a_user[BUFFER_LEN];
        char msg[BUFFER_LEN*2];
        //se ricevo il messaggio di chiusura chat effettuo
        //le istruzioni necessarie per uscire dalla chat
        if(!strcmp("\\q",tmp)){
            chiudiChat();
            return;
        }
        else if(!strcmp("\\u",tmp)){
            inviaRichiestaUserOnline(Device.port);
            return;
        }
        else if(!strcmp("\\a", tmp)){
            sscanf(buffer, "%s %s", tmp, username);
            elem = controllaRubrica(username);
            if(elem != NULL){
                DEBUG_PRINT(("user %s %d in rubrica ", elem->username, elem->port));
                if(controllaListaUtentiChat(elem)){
                    printf("già in chat con %s", elem->username);
                    return;
                }
                elem->next =NULL;
                inserisciUser(elem, &Device.lista_contatti_chat);
                printf("in chat con %s\n", Device.lista_contatti_chat->username);
                sprintf(tmp, "%s %s", Device.username, username);
                sd = creaConnessioneTCP(Device.server_port);
                inviaMessaggioTCP(sd, "CHR", (void*)tmp, strlen(tmp));
                elem = Device.lista_contatti_chat;
                for(elem; elem!=NULL; elem = elem->next){
                    DEBUG_PRINT(("!!!!%s!!!!", elem->username));
                }
            }
            return;
        }

        //creo un messaggio di intestazione nel formato
        //(sender_name)->(dest_name): 
        //e la stampo a video
        sprintf(da_user_a_user, "(%s)->", Device.username);
        elem = Device.lista_contatti_chat;
        for(elem; elem!=NULL; elem = elem->next){
            strcat(da_user_a_user, "(");
            strcat(da_user_a_user, elem->username);
            strcat(da_user_a_user, ")");
            if(elem->next==NULL){
                strcat(da_user_a_user, ": ");
            }
        }
        printf("%s", da_user_a_user);
        
        //scrivo il messaggo di chat dentro buffer e creo dinamicamente una stringa
        //che andrà a contenere il messaggio di intestazione e il messaggio di chat
        sscanf(buffer, "%s", tmp);
       // msg = malloc(strlen(buffer)+strlen(da_user_a_user));
        strcpy(msg, da_user_a_user);
        strcat(msg, buffer);
        DEBUG_PRINT(("%s", msg));

        //invio il messaggio al server e libero la memoria precedentemente allocata
        elem = Device.lista_contatti_chat;
        for(elem; elem!=NULL; elem = elem->next){
            inviaMessaggioDiChat(elem->username,Device.port, msg);
        }
        //if(msg)
           // free(msg);
    }    
}

/*
    visualizzaCronologiaMessaggi è la funzione che dati count messaggi in arrivo su sd
    li stampa a video
*/
void visualizzaCronologiaMessaggi(int sd, int count){
    int i;
    char* msg;
    char* tmp;
    char operazione[OP_LEN];
    time_t time;
    for(i = 0; i<count; ++i){
        riceviMessaggioTCP(sd, operazione, (void*)&msg);
            printf(msg);
    }
    printf("\n fine cronologia \n");
}

/*
    controlloInputTCP è la funzione che gestisce i messaggi in ingresso tramite
    una connessione TCP e in base al tipo di operazione agisce in accordo a esso.
    La funzione prende in ingresso il socket descriptor della connessione in ingresso
    Spegazione dei codici ricevuti
    HGR --> ricevuto dal server un messaggio di risposta alla richiesta di hanging
    SWR --> ricevuto dal server un messaggio di risposta alla richiesta di show
    LOG --> ricevuto dal server un messaggio per visualizzare il log di chat
    CHT --> ricevuto dal server un messaggio di chat
    SRC --> ricevuto dal server una notifica che i messaggi pendenti sono stati letti
    PSM --> ricevuto dal server che vi era una notifica in attesa
    UON --> ricevuto dal server un messaggio per visualizzare gli utenti online
    SHR --> ricevuto da un device la notifica di inizio trasferimento file
*/
void controlloInputTCP(int sd){
    char operazione[OP_LEN];
    char username[USER_LEN];
    char password[USER_LEN];
    int port;
    char* msg;
    char* file_path;
    int ret, ctrl;
    FILE* fp;

    //faccio pulizia della memoria contenuta in operazione
    //e ricevo il messaggio
    memset(operazione, '\0', OP_LEN);
    ret = riceviMessaggioTCP(sd, operazione, (void*)&msg);
    if(ret<0){
        perror("errore in fase di ricezione del messaggio:");
        exit(-1);
    }

    //in base al cotenuto di operazione agisco diversamente
    if(!strcmp("HGR", operazione)){
        printf("%s\n", msg);
    }
    else if(!strcmp("SWR", operazione)){
        sscanf(msg, "%d", &ctrl);
        DEBUG_PRINT(("%d %d", sd, ctrl);)
        visualizzaCronologiaMessaggi(sd, ctrl);
    }
    else if(!strcmp("LOG", operazione)){
        sscanf(msg, "%d", &ctrl);
        DEBUG_PRINT(("%d %d", sd, ctrl);)
        visualizzaCronologiaMessaggi(sd, ctrl);
    }
    //Se ricevo un messaggio di chat e lo user è in chat allora stampo a 
    //video il messaggio, altrimenti segnalo al server che lo user non
    //può ricevere messaggi attualmente (user offline o non in chat)
    else if(!strcmp("CHT", operazione)){
        if(Device.chatting && Device.logged){
            inviaMessaggioTCP(sd, "OK!", "", 0);
            printf("%s\n", msg);
        }
        else{
            DEBUG_PRINT(("NOT"));
            inviaMessaggioTCP(sd, "NOT", "", 0);
        }

    }
    else if(!strcmp("SRC", operazione)){
        printf("messaggio letto\n");
    }
    else if(!strcmp("PMS", operazione)){
        printf("%s ha letto i messaggi pendenti", msg);
    }
    else if(!strcmp("UON", operazione)){
        //if(!strcmp(msg, Device.username))
          //  return;
        printf("%s\n", msg);
    }
    else if (!strcmp("SHR", operazione)){ 
        DEBUG_PRINT(("%s %d %d", msg, ret, strlen(msg)));
        //creo un nuovo file e lo apro in scrittura binaria
        sprintf(file_path, "./%s/ricevuto_%s", Device.username, msg);
        DEBUG_PRINT(("salvataggio del file in %s", file_path));
        fp = fopen(file_path, "wb");
        if (!fp) {
            perror("errore nella creazione del file");
            return;
        }
        //trasmissione di un chunk di dati appartenente al file
        do{ 
            free(msg);
            memset(operazione, '\0', OP_LEN);
            DEBUG_PRINT(("preparo la ricezione"));
            ret =riceviMessaggioTCP(sd, operazione, (void*)&msg);
            DEBUG_PRINT(("ricevuto %s %s",operazione, msg));
            
            if (strcmp("FLT", operazione))
                break;
            fwrite((void*)msg, 1, ret, fp);

        }
        //fine del file e messaggio da inserire in chat 
        while(strcmp("EOT", operazione)); 
        fclose(fp);
    }

}
/*
    controlloInput è la funzione principale che gestisce lo IOMultiplexing,
    sfrutta la struttura dati file_descriptor_set fds per la gestione dei
    file descriptor
*/
void controlloInput(){
    int ret, newfd, listener, i;
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
    my_addr.sin_port = htons(Device.port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(listener, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if (ret < 0) {
        perror("Bind listener non riuscita\n");
        exit(0);
    }

    //se la bind viene eseguita con successo, viene creata la coda di ingresso,
    //vengono resettati i file descriptor e successivamente inseriti i fd per
    //il listener e STDIN
    listen(listener, 10);
    DEBUG_PRINT(("in ascolto sulla porta %d", Device.port));

    FD_ZERO(&(fds.master));
    FD_ZERO(&(fds.read_fds));

    FD_SET(STDIN, &(fds.master));
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
        if (i > 0) {
            
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

                    //altrimenti se i == STDIN vuol dire che è stato rilevato un
                    //input da tastiera per cui passo il buffer di ingresso
                    //alla funzione che si occupa di gestire lo STDIN
                    else if (i == STDIN) { 
                        if (read(STDIN, buffer, 4096)){
                            DEBUG_PRINT(("STDIN"));
                            controlloInputTastiera(buffer);
                        }
                            
                    }

                    //altrimenti vuol dire che è stato rilevato un messaggio TCP
                    //per cui viene creato un figlio per gestire la connessione
                    else { 
                        pid = fork();
                        if (pid == -1) {
                            perror("Errore durante la fork: ");
                            return;
                        }
                        //figlio
                        if (pid == 0) {
                            DEBUG_PRINT(("TCP"));
                            close(listener);
                            controlloInputTCP(i);
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
    if(argc != 1){
        Device.port = atoi(argv[1]);
    }
    DEBUG_PRINT(("%d\n", Device.port));
    controlloInput();
}
