#include<time.h>
#define LEN 30

typedef struct UserEntry_struct UserEntry;
struct UserEntry_struct {
    char user_name[LEN]; // username dell'utente registrato
    char password[LEN]; // password dell'utente registrato
    int port; // la porta su cui è raggiungibile l'utente
            // ha senso soltanto se timestamp_login > timestamp_logout
    time_t timestamp_login; // timestamp di login
    time_t timestamp_logout; // timestamp di logout
    int tentativi_connessione; // chances entro le quali il device è ritenuto online
                // rispondere alla richiesta di heartbeat ripristina le chances
    UserEntry* next;
};

void inserisciUser(UserEntry *user, UserEntry **testa);