#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define USERNAME_LEN 30

typedef struct UserPending_struct UserPending;

/*
    UserPending_struct è la struttura che tiene conto delle informazioni necessarie
    per inviare la notifica di avvenuta lettura dei messaggi inviati al sender qualora
    quest'ultimo non fosse online al momento della lettura
*/
struct UserPending_struct{
    char username_sender[USERNAME_LEN];
    char username_receiver[USERNAME_LEN];
    int port_sender;
    UserPending* next;
};

/*
    inserisciUser è una funzione di utilità che inserisce user nella
    lista testa
*/
void inserisciUserPending(UserPending *user, UserPending **testa){
    UserPending *p = *testa;
    while(p != NULL && p->next != NULL) {
        if(!strcmp(user->username_sender, p->username_sender) && !strcmp(user->username_receiver, p->username_receiver)){
            return;
        }
        p = p->next;
    }
    if (p != NULL)
        p->next = user;
    else
        *testa = user;
}