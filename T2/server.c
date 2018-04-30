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
#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

// Defined constants
#define MAX_MSG_LEN 100

// Global variables
unsigned int num_room_seats;
unsigned int num_tickets_offices;
unsigned long int open_time;

typedef struct{
    unsigned int client;
    unsigned int num_seats;
    unsigned int *preferred_seats;
} request;

// Functions

int main(int argc, char const *argv[]) {
    int fd,         // File Descriptor
        fd_dummy,   // Dummy File Descriptor
        n;          // Number of characters read
    char str[100];

    // Usage
    if(argc != 4){
        fprintf(stderr, "Usage: server <num_room_seats> <num_tickets_offices> <open_time>\n");
        return 1;
    }

    // Set global variables
    num_room_seats = strtol(argv[1], NULL, 10);
    num_tickets_offices = strtol(argv[2], NULL, 10);
    open_time = strtol(argv[3], NULL, 10);

    // Make FIFO 'ŕequests'
    if(mkfifo("requests", 0660) == 0){
        fprintf(stdout, "Fifo requests created successfuly\n");
    }
    else{
        if(errno == EEXIST) fprintf(stderr, "Fifo requests already exists\n");
        else fprintf(stderr, "Can't create fifo requests\n");
    }

    // Open FIFO 'requests' for reading
    if((fd = open("requests", O_RDONLY)) == -1){
        fprintf(stderr, "Error opening FIFO 'requests' in READONLY mode");
        exit(1);
    }

    // Open FIFO 'requests' for writing -> prevent waiting
    if((fd_dummy = open("requests", O_WRONLY)) == -1){
        fprintf(stderr, "Error opening FIFO 'requests' in WRITEONLY mode");
        exit(1);
    }

    // Receive requests
    do {
        n = read(fd, str, MAX_MSG_LEN);
        printf("Message: %s\n", str);
    } while(n > 0);

    // Close FIFO 'requests'
    close(fd);
    close(fd_dummy);

    // Destroy FIFO 'requests'
    if(unlink("requests") < 0)
        fprintf(stderr, "Error destroying FIFO 'requests'\n");
    else
        fprintf(stdout, "FIFO 'requests' has been destroyed\n");

    // Exit program successfuly
    exit(0);
}
