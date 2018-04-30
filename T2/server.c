// standard C libraries
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// UNIX/Linux libraries
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

// Global variables
unsigned int num_room_seats;
unsigned int num_tickets_offices;
unsigned long int open_time;

// Functions
int readline(int fd, char* str);

int main(int argc, char const *argv[]) {
    int fd;
    char str[100];

    if(argc != 3){
        fprintf(stderr, "Usage: server <num_room_seats> <num_tickets_offices> <open_time>\n");
        return 1;
    }

    if(mkfifo("requests", 0660) == 0){
        fd = open("requests", O_RDONLY);

        putchar('\n');
        while(readline(fd, str)) printf("%s", str);
        close(fd);
        return 0;
    }
    else{
        fprintf(stderr, "Error opening fifo\n");
        return 1;
    }
}

int readline(int fd, char* str){
    int n;

    do {
        n = read(fd, str, 1);
    } while(n > 0 && *str++ != '\0');

    return (n>0);
}
