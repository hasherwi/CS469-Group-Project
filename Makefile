CC := gcc
LDFLAGS := -lssl -lcrypto
AUDIOFLAGS := -lmpg123 -lao
UNAME := $(shell uname)
ARCH := $(shell uname -m)

ifeq ($(UNAME), Darwin)
  ifeq ($(ARCH), arm64)
	CFLAGS := -I/opt/homebrew/include -L/opt/homebrew/lib
    AUDIOFLAGS :=  $(CFLAGS) $(AUDIOFLAGS)
  else
    CFLAGS := -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib
  endif
endif

all: client server playaudio

playaudio: playaudio.o
	$(CC) $(AUDIOFLAGS) -o playaudio playaudio.o $(AUDIOFLAGS)

playaudio.o: playaudio.c
	$(CC) $(AUDIOFLAGS) -c playaudio.c

client: client.o
	$(CC) $(CFLAGS) -o client client.o $(LDFLAGS)

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

server: server.o
	$(CC) $(CFLAGS) -o server server.o $(LDFLAGS)

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

clean:
	rm -f server server.o client client.o playaudio playaudio.o
