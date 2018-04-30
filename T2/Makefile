CC 		= gcc
CFLAGS 	= -Wall -Wextra -Werror
RM 		= rm -f

default: all

all: Server Client

Server: server.c
	@$(CC) $(CFLAGS) -o server server.c

Client: client.c
	@$(CC) $(CFLAGS) -o client client.c

clean:
	@$(RM) server client
