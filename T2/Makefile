CC 		= gcc
CFLAGS 	= -Wall -Wextra  -D_REENTRANT -pthread
RM 		= rm -f

default: all

all: Server Client Start

Server: server.c
	@$(CC) $(CFLAGS) -o server server.c

Client: client.c
	@$(CC) $(CFLAGS) -o client client.c

Start: start.c
	@$(CC) $(CFLAGS) -o start start.c

clean:
	@$(RM) server client
