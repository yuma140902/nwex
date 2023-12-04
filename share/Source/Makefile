CC = gcc
CFLAGS = -Wall -Wextra

receiver.out: ./file_receiver.c util.o
	$(CC) $(CFLAGS) -c ./file_receiver.c
	$(CC) $(CFLAGS) file_receiver.o util.o -o receiver.out

sender.out: ./file_sender.c util.o
	$(CC) $(CFLAGS) -c ./file_sender.c
	$(CC) $(CFLAGS) file_sender.o util.o -o sender.out

relay.out: ./file_relay.c
	$(CC) $(CFLAGS) -c ./file_relay.c
	$(CC) $(CFLAGS) file_relay.o -o relay.out

util.o: ./util.c
	$(CC) $(CFLAGS) -c ./util.c
