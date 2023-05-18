# make rule primaria con dummy target ‘all’--> non crea alcun file all ma fa un complete build
# che dipende dai target client e server scritti sotto
#all: client server
# make rule per il client
#client: client.o
#gcc –Wall client.o –o client
# make rule per il server

# pulizia dei file della compilazione (eseguito con ‘make clean’ da terminale)

all: server device

server: UserEntry.o TCP.o UserPending.o server.o 
	gcc -Wall UserPending.o UserEntry.o TCP.o server.o -o server

UserEntry.o: utility/UserEntry.c
	gcc -c -g utility/UserEntry.c

UserPending.o: utility/UserPending.c
	gcc -c -g utility/UserPending.c

device: TCP.o UserContact.o device.o
	gcc -Wall TCP.o UserContact.o device.o -o device

TCP.o: utility/TCP.c
	gcc -c -g utility/TCP.c

UserContact.o: utility/UserContact.c
	gcc -c -g utility/UserContact.c
clean: 
	rm *o
	rm device server
