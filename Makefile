CC := gcc
LDFLAGS := -lssl -lcrypto
AUDIOFLAGS := -lmpg123 -lao
UNAME := $(shell uname)
ARCH := $(shell uname -m)
ARCH := $(shell uname -m)

ifeq ($(UNAME), Darwin)
  ifeq ($(ARCH), arm64)
    CFLAGS := -I/opt/homebrew/include -L/opt/homebrew/lib
  else
    CFLAGS := -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib
  endif
endif

all: client server

client: client.o playaudio.o CommunicationConstants.h
	$(CC) $(CFLAGS) -o client client.o playaudio.o $(LDFLAGS) $(AUDIOFLAGS) -lpthread

client.o: client.c playaudio.h
	$(CC) $(CFLAGS) -c client.c 

playaudio.o: playaudio.c playaudio.h
	$(CC) $(CFLAGS) -c playaudio.c

server: server.o CommunicationConstants.h
	$(CC) $(CFLAGS) -o server server.o $(LDFLAGS)

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -f server server.o client client.o playaudio.o
	rm -f server server.o client client.o playaudio playaudio.o
