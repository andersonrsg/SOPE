CC 		= gcc
CFLAGS 	= -Wall -g
RM 		= rm -f

default: all

all: Simgrep

Simgrep: simgrep.c
	@$(CC) $(CFLAGS) -o simgrep simgrep.c

clean:
	@$(RM) simgrep
