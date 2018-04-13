CC 		= gcc
CFLAGS 	= -Wall -Wextra -Werror
RM 		= rm -f

default: all

all: Simgrep

Simgrep: simgrep.c
	@$(CC) $(CFLAGS) -o simgrep simgrep.c

clean:
	@$(RM) simgrep
