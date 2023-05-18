#define OP_LEN 4
int creaConnessioneTCP(int server_port);
int inviaMessaggioTCP(int sd, char operazione[OP_LEN], void* buffer, int len);
int riceviMessaggioTCP(int sd,char operazione[OP_LEN], void** buffer);