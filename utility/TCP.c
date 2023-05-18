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
#define DEBUG_ON

#ifdef DEBUG_ON
    #define DEBUG_PRINT(x) printf("[NETWORK]: "); printf x; printf("\n");
#endif

#define OP_LEN 4

/*
    creaConnessioneTCP prende in ingresso un numero di porta
    e restituisce il socket descriptor su cui è stata creata la 
    connessione
*/
int creaConnessioneTCP(int port)
{
    struct sockaddr_in connection_addr;
    int sd;

    if(port == -1)
        return -1;
        
    sd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&connection_addr, 0, sizeof(connection_addr));
    connection_addr.sin_family = AF_INET;
    connection_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &connection_addr.sin_addr);

    if (connect(sd, (struct sockaddr *)&connection_addr, sizeof(connection_addr)) < 0) {
        perror("Errore in fase di connessione");
        return -1;
    }
    return sd;
}

/*
    inviaMessaggioTCP prende in ingresso il socket descriptor che identifica
    la connessione su cui inviare dei valori contenuti in buffer di lunghezza len,
    operazione è un stringa di 4 char, compreso il byte di escape, che identifica
    per il reciver il tipo di operazione da effettuare sui dati ricevuti.
    La funzione ritorna il numero di byte inviati
*/

int inviaMessaggioTCP(int sd, char operazione[OP_LEN], void* buffer, int len){
    u_int32_t msg_len;
    int ret;
    DEBUG_PRINT(("%s", operazione));
    //viene inviato il codice che identifica l'operazione,
    //non viene inviata prima la lunghezza della stringa
    //in quanto è sempre composta da 4 elementi
    ret = send(sd, (void*)operazione, OP_LEN,0);
    if(ret<0){
        perror("errore in fase di send operazione: ");
        exit(-1);
    }
    //viene inviata la lunghezza del messaggio contenuto in buffer
    //così che il receiver sa in anticipo quali sono le sue dimensioni
    msg_len = htonl(len);
    ret = send(sd, (void*)&msg_len, sizeof(u_int32_t), 0);
    if(ret<0){
        perror("errore in fase di send msg_len: ");
        exit(-1);
    }

    //viene inviato l'effettivo messaggio
    ret = send(sd, buffer, len, 0);
    if(ret<0){
        perror("errore in fase di send buffer: ");
        exit(-1);
    }
    return ret;
}
/*
    riceviMessaggioTCP prende in ingresso il socket descriptor che identifica
    la connessione su cui ricevere dei valori da piazzare in buffer,
    operazione è un stringa di 4 char, compreso il byte di escape, che identifica
    per il reciver il tipo di operazione da effettuare sui dati ricevuti.
    La funzione ritorna il numero di byte ricevuti
*/
int riceviMessaggioTCP(int sd, char operazione[OP_LEN], void** buffer){
    int ret, len;
    uint32_t lmsg;
    //effettuo la pulizia della memoria di operazione
    //e ci scrivo dentro i dati ricevuti
    memset(operazione, '\0', OP_LEN);
    ret = recv(sd, (char*)operazione, OP_LEN, 0);
    if (ret< OP_LEN) {
        perror("errore protocollo: ");
        DEBUG_PRINT(("Errore in fase di ricezione protocollo, ricevuto %s %d", operazione, ret));
        exit(-1);
    }
    //ricevo la lunghezza del messaggio di dati in ingresso
    ret = recv(sd, (void *)&lmsg, sizeof(uint32_t), 0);
    if (ret < 0) {
        perror("Errore in fase di ricezione lunghezza messaggio");
        exit(-1);
    }
    //converto la lunghezza ricevuta nel formato locate e se
    //è maggiore di zero passo a salvare i dati in ingresso
    len = ntohl(lmsg);
    DEBUG_PRINT(("lunghezza di len: %d", len));
    if(len>0){
        //alloco memoria per il buffer pari alla lunghezza della stringa
        //dei dati in ingresso e ricevo i dati
        *buffer = malloc(len);
        memset(*buffer, 0, len);
        ret = recv(sd, *buffer, len, 0);
        if (ret < 0) {
            perror("errore in fase di ricezione del messaggio: ");
            exit(1);
        }
    }
    return ret;
}