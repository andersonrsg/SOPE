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
#include <pthread.h>

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
} requests;

// Functions
int get_number_size(size_t number);
char* makeClientFIFO(int pid);
void requestHandler(char* request);

int main(int argc, char const *argv[]) {
    int fd,         // File Descriptor
        fd_dummy,   // Dummy File Descriptor
        n;          // store the number of char's read
    char request[MAX_MSG_LEN+1];


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
    while ((n = read(fd, request, MAX_MSG_LEN)) > 0) {
        printf("received request: %s\n", request);
        requestHandler(request);
    }

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

void requestHandler(char* request){
    requests rq;
    char *token,
         *delim = " ";
    int *seats = NULL,
        i = 0;

    // get client PID
    printf("[REQUEST HANDLER]: extracting Client PID from request\n");
    token = strtok(request, delim);
    rq.client = strtol(token, NULL, 10);
    printf("[REQUEST HANDLER]: extracted Client PID from request: %d\n", rq.client);

    printf("[REQUEST HANDLER]: extracting Client desired number of seats from request\n");
    // get client desired number of seats
    token = strtok(NULL, delim);
    rq.num_seats = strtol(token, NULL, 10);
    printf("[REQUEST HANDLER]: extracted Client desired number of seats from request: %d\n", rq.num_seats);

    printf("[REQUEST HANDLER]: extracting Client desired seats from request\n");
    // get the desired seats
    while(token != NULL){
        token = strtok(NULL, delim);
        seats = (int*)realloc(seats, ++i * sizeof(int));
        seats[i-1] = strtol(token, NULL, 10);
    }

    rq.preferred_seats = memcpy(rq.preferred_seats, seats, i * sizeof(unsigned int));
    printf("[REQUEST HANDLER]: extracted Client desired number of seats from request\n");

    printf("Cliente: %d\n", rq.client);
    printf("Num seats: %d\n", rq.client);
    printf("desired seats: ");
    for(int j = 0; j < i; j++){
        printf("%d ", rq.preferred_seats[j]);
    }
    printf("\n");
}


int get_number_size(size_t number){
    int i = 0;
    do {
        number /= 10;
        i++;
    } while(number > 0);
    return i;
}

char* makeClientFIFO(int pid){
    char* fifo = (char*)malloc(strlen("ans") + get_number_size(pid) + 1);

    sprintf(fifo, "ans%d", pid);

    return fifo;
}
