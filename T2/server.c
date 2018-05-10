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
#define MAX_CLI_SEATS 10

// Global variables
unsigned int num_room_seats;        // Number of seats
unsigned int num_tickets_offices;   // Number of ticket offices (threads)
unsigned long int open_time;        // ticket offices open time (seconds)
unsigned int *seats;                // Seats

typedef struct{
    unsigned int client;
    unsigned int num_wanted_seats;
    unsigned int *preferred_seats;
} requests;

// Functions
int get_number_size(size_t number);
char* getClientFIFO(int pid);
requests* requestDisassembler(char* request);
void *requestHandler(void *request);


// MAIN
int main(int argc, char const *argv[]) {
    int fd,         // File Descriptor
        fd_dummy,   // Dummy File Descriptor
        n;          // store the number of char's read
    char request[MAX_MSG_LEN+1];
    requests *rq;


    // Usage
    if(argc != 4){
        fprintf(stderr, "Usage: server <num_room_seats> <num_tickets_offices> <open_time>\n");
        return 1;
    }

    // Set global variables
    num_room_seats = strtol(argv[1], NULL, 10);
    num_tickets_offices = strtol(argv[2], NULL, 10);
    open_time = strtol(argv[3], NULL, 10);
    memset(seats, 0, num_room_seats * sizeof(unsigned int));

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
        rq = requestDisassembler(request);

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

requests* requestDisassembler(char* request){
    requests *rq = malloc(sizeof(requests));
    char *token,
         *delim = " ";
    int i = 0;

    // get client PID
    printf("[REQUEST HANDLER]: extracting Client PID from request\n");
    token = strtok(request, delim);
    rq->client = strtol(token, NULL, 10);
    printf("[REQUEST HANDLER]: extracted Client PID from request: %d\n", rq->client);

    printf("[REQUEST HANDLER]: extracting Client desired number of seats from request\n");
    // get client desired number of seats
    token = strtok(NULL, delim);
    rq->num_wanted_seats = strtol(token, NULL, 10);
    printf("[REQUEST HANDLER]: extracted Client desired number of seats from request: %d\n", rq->num_wanted_seats);

    printf("[REQUEST HANDLER]: extracting Client desired seats from request\n");
    // get the desired seats
    token = strtok(NULL, delim);
    while(token != NULL){
        rq->preferred_seats = realloc(rq->preferred_seats, ++i * sizeof(int));
        rq->preferred_seats[i-1] = strtol(token, NULL, 10);
        token = strtok(NULL, delim);
    }
    printf("[REQUEST HANDLER]: extracted Client desired number of seats from request\n");

    printf("Cliente: %d\n", rq->client);
    printf("Num seats: %d\n", rq->num_wanted_seats);
    printf("desired seats: ");
    for(int j = 0; j < i; j++){
        printf("%d ", rq->preferred_seats[j]);
    }
    printf("\n");

    return rq;
}

void *requestHandler(void *request){
    requests *rq = (requests*)request;

    //validate request
    if(rq->num_wanted_seats > MAX_CLI_SEATS){
        fprintf(stderr, "[TICKET OFFICE %ld]: Number of seats wanted higher than permited (%d)\n", pthread_self(), MAX_CLI_SEATS);
        return NULL;
    }
    if((sizeof(rq->preferred_seats) / sizeof(unsigned int)) < rq->num_wanted_seats){
        fprintf(stderr, "[TICKET OFFICE %ld]: Number of preferred seats smaller than number of wanted seats (%d)\n", pthread_self(), rq->num_wanted_seats);
        return NULL;
    }
    if((sizeof(rq->preferred_seats) / sizeof(unsigned int)) > MAX_CLI_SEATS){
        fprintf(stderr, "[TICKET OFFICE %ld]: Number of preferred seats bigger than permited (%d)\n", pthread_self(), MAX_CLI_SEATS);
        return NULL;
    }
    for (unsigned int i = 0; i < (sizeof(rq->preferred_seats) / sizeof(unsigned int)); i++) {
        if(rq->preferred_seats[i] > num_room_seats){
            fprintf(stderr, "[TICKET OFFICE %ld]: Preferred seat %d do not exist\n", pthread_self(), rq->preferred_seats[i]);
            return NULL;
        }
    }

    // Reservate seats

    return NULL;
}

int get_number_size(size_t number){
    int i = 0;
    do {
        number /= 10;
        i++;
    } while(number > 0);
    return i;
}

char* getClientFIFO(int pid){
    char* fifo = (char*)malloc(strlen("ans") + get_number_size(pid) + 1);

    sprintf(fifo, "ans%d", pid);

    return fifo;
}
