#define USERNAME_LEN 30

typedef struct UserPending_s UserPending;
struct UserPending_s{
    char username_sender[USERNAME_LEN];
    char username_receiver[USERNAME_LEN];
    int port_sender;
    UserPending* next;
};

void inserisciUserPending(UserPending *user, UserPending **testa);