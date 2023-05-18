typedef struct UserContact_s UserContact;
struct UserContact_s {
    char username[30];
    int port;
    UserContact* next;
};

void inserisciUser(UserContact *user, UserContact **testa);