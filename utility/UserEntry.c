#include<time.h>
#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#define LEN 30

typedef struct UserEntry_struct UserEntry;

/*
    UserEntry_struct è la struct usata dal Server per tenere traccia
    dei vari utenti iscritti al servizio
*/
struct UserEntry_struct {
    char user_name[LEN];        //nome utente
    char password[LEN];         //password utente (salvata in chiaro)
    int port;                   //porta su cui è attualmente in ascolto l'utente
    time_t timestamp_login;     //timestamp dell'ultimo login
    time_t timestamp_logout;    //timestamp dell'ultimo logout
    int tentativi_connessione;  //counter che tiene traccia dei tentativi di connessione effettuati
    UserEntry* next;
};

/*
    inserisciUser è una funzione di utilità che inserisce user nella
    lista testa
*/
void inserisciUser(UserEntry *user, UserEntry **testa){
    UserEntry *p = *testa;
    while(p != NULL && p->next != NULL) {
        p = p->next;
    }
    if (p != NULL)
        p->next = user;
    else
        *testa = user;
}