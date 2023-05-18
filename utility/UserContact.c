#include<stdbool.h>
#include<stdlib.h>


typedef struct UserContact_struct UserContact;

/*
    UserContact_struct è la struttura che tiene conto dei contatti in rubrica
    dei vari utenti, tiene in memoria il nome utente e la porta anche se
    di default è impostata a 0 in quanto è sconosciuta, ma è comunque parte
    della struttura in quanto nelle operazioni di share è imporante salvarsi
    la porta dell'utente a cui si vuole inviare il file
*/
struct UserContact_struct {
    char username[30];
    int port;
    UserContact* next;
};

/*
    inserisciUser è una funzione di utilità che inserisce user nella
    lista testa
*/
void inserisciUser(UserContact *user, UserContact **testa){
    UserContact *p = *testa;
    while(p != NULL && p->next != NULL) {
        p = p->next;
    }
    if (p != NULL)
        p->next = user;
    else
        *testa = user;
}