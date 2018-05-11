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

// Defined Error constants
#define INVALID_NUM_SEAT         "-1"   // Number of desired seats is higher than permited
#define INVALID_NUM_PREF_SEAT    "-2"   // Number of preferred seats is invalid
#define INVALID_SEAT_ID          "-3"   // ID of preferred seat is invalid
#define PARAM_ERROR              "-4"   // Any other parameter error
#define BOOKING_FAILED           "-5"   // Could not reservate all the desired seats
#define FULL_ROOM                "-6"   // The room is full

// Global variables
int num_room_seats;                 // Number of seats
int num_tickets_offices;            // Number of ticket offices (threads)
unsigned long int open_time;        // ticket offices open time (seconds)
int *seats;                         // Seats

typedef struct{
    int client;
    int num_wanted_seats;
    int *preferred_seats;
} requests;

// Functions
int get_number_size(size_t number);
char* getClientFIFO(int pid);
requests* requestDisassembler(char* request);
void *requestHandler(void *request);
int validateRequest(int *seats, requests *request, int fd);
int isSeatFree(int *seats, int seatNum);
void bookSeat(int *seats, int seatNum, int clientId);
void freeSeat(int *seats, int seatNum);


// MAIN
int main(int argc, char const *argv[]) {
    int fd;         // File Descriptor
    char request[MAX_MSG_LEN+1];
    requests *rq;


    // Usage
    if(argc != 4){
        fprintf(stderr, "Usage: server <num_room_seats> <num_tickets_offices> <open_time>\n");
        return 1;
    }

    printf("[MAIN]: Setting global parameters\n");
    // Set global variables
    num_room_seats = strtol(argv[1], NULL, 10);
    num_tickets_offices = strtol(argv[2], NULL, 10);
    open_time = strtol(argv[3], NULL, 10);
    seats = malloc(num_room_seats * sizeof(int));
    memset(seats, 0, num_room_seats * sizeof(int));

    // Make FIFO 'ŕequests'
    printf("[MAIN]: Attempting to create FIFO 'requests'\n");
    if(mkfifo("requests", 0660) == 0){
        printf("[MAIN]: FIFO 'requests' created successfuly\n");
    }
    else{
        if(errno == EEXIST) fprintf(stderr, "[MAIN]: FIFO 'requests' already exists\n");
        else {
            fprintf(stderr, "[MAIN]: Can't create FIFO 'requests'\n");
            exit(1);
        }
    }

    // Open FIFO 'requests' for reading and prevent waiting
    printf("[MAIN]: Attempting to open FIFO 'requests'\n");
    if((fd = open("requests", O_RDONLY | O_NONBLOCK)) == -1){
        fprintf(stderr, "[MAIN]: Error opening FIFO 'requests' in READONLY mode");
        exit(1);
    }
    printf("[MAIN]: successfuly opened FIFO 'requests'\n");

    printf("[MAIN]: Ready to recieve requests\n");
    // Receive requests
    while (1) {
        if(read(fd, request, MAX_MSG_LEN) > 0){
            printf("recieved request: %s\n", request);
            rq = requestDisassembler(request);
            requestHandler(rq);
        }
    }

    // Close FIFO 'requests'
    close(fd);

    // Destroy FIFO 'requests'
    if(unlink("requests") < 0)
        fprintf(stderr, "[MAIN]: Error destroying FIFO 'requests'\n");
    else
        fprintf(stdout, "[MAIN]: FIFO 'requests' has been destroyed\n");

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
    printf("[REQUEST HANDLER]: extracted Client PID from request\n");

    printf("[REQUEST HANDLER]: extracting Client desired number of seats from request\n");
    // get client desired number of seats
    token = strtok(NULL, delim);
    rq->num_wanted_seats = strtol(token, NULL, 10);
    printf("[REQUEST HANDLER]: extracted Client desired number of seats from request\n");

    printf("[REQUEST HANDLER]: extracting Client desired seats from request\n");
    // get the desired seats
    rq->preferred_seats = NULL;
    token = strtok(NULL, delim);
    while(token != NULL){
        rq->preferred_seats = realloc(rq->preferred_seats, ++i * sizeof(int));
        rq->preferred_seats[i-1] = strtol(token, NULL, 10) - 1;
        token = strtok(NULL, delim);
    }
    rq->preferred_seats = realloc(rq->preferred_seats, ++i * sizeof(int));
    rq->preferred_seats[i-1] = -1;
    printf("[REQUEST HANDLER]: extracted Client desired number of seats from request\n");

    printf("Cliente: %d\n", rq->client);
    printf("Num seats: %d\n", rq->num_wanted_seats);
    printf("desired seats: ");
    for(int j = 0; rq->preferred_seats[j] != -1; j++){
        printf("%d ", rq->preferred_seats[j] + 1);
    }
    printf("\n");

    return rq;
}

void *requestHandler(void *request){
    requests *rq = (requests*)request;
    pthread_t tid = pthread_self();
    int num_preferred_seats = 0,
        fd;
    char *fifo = getClientFIFO(rq->client),
         msg[MAX_MSG_LEN];

    if((fd = open(fifo, O_WRONLY)) < 0){
        fprintf(stderr, "[TICKET OFFICE %ld]: Error opening FIFO '%s'\n", tid, fifo);
        return NULL;
    }

    // Validate request
    if(validateRequest(seats, rq, fd)){

        for (int i = 0; rq->preferred_seats[i] != -1; i++) {
            num_preferred_seats++;
        }

        // Reservate seats
        for (int i = 0; i < num_preferred_seats && rq->num_wanted_seats; i++) {
            if(isSeatFree(seats, rq->preferred_seats[i])){
                bookSeat(seats, rq->preferred_seats[i], rq->client);
                printf("[TICKET OFFICE %ld]: Seat %d booked\n", tid, rq->preferred_seats[i] + 1);
                rq->num_wanted_seats--;
            }
            else{
                printf("[TICKET OFFICE %ld]: Seat %d already taken\n", tid, rq->preferred_seats[i] + 1);
                rq->preferred_seats[i] = -1;
            }
        }


        if(rq->num_wanted_seats){   // Could not reservate the total number of desired seats
            printf("[TICKET OFFICE %ld]: Could not book the total number of seats desired. Rolling back booking operation\n", tid);
            write(fd, BOOKING_FAILED, sizeof(BOOKING_FAILED));
            // Rollback operation
            for (int i = 0; i < num_preferred_seats; i++) {
                if (rq->preferred_seats[i] != -1) {
                    freeSeat(seats, rq->preferred_seats[i]);
                }
            }
        }
        else{   // successfuly reservated all desired seats
            printf("[TICKET OFFICE %ld]: successfuly booked seats", tid);
            sprintf(msg, "%d", rq->num_wanted_seats);

            for (int i = 0, j = 1; i < num_preferred_seats; i++, j+=2) {
                if (rq->preferred_seats[i] != -1) {
                    printf(" %d", rq->preferred_seats[i] + 1);

                    sprintf(&msg[j], " %d", rq->preferred_seats[i] + 1);
                }
            }

            write(fd, msg, sizeof(msg));
        }

        printf("\n");
    }

    return NULL;
}

int validateRequest(int *seats, requests *request, int fd){
    int room_full = 1,
        num_preferred_seats = 0;
    pthread_t tid = pthread_self();

    // Check if room is full
    for (int i = 0; i < num_room_seats; i++) {
        if(!seats[i]){
            room_full = 0;
        }
    }
    if(room_full){
        fprintf(stderr, "[TICKET OFFICE %ld]: Room is full\n", tid);
        write(fd, FULL_ROOM, sizeof(FULL_ROOM));
        return 0;
    }

    // Check if desired number of seats if valid
    if(request->num_wanted_seats > MAX_CLI_SEATS){
        fprintf(stderr, "[TICKET OFFICE %ld]: Number of seats wanted higher than permited (%d)\n", tid, MAX_CLI_SEATS-1);
        write(fd, INVALID_NUM_SEAT, sizeof(INVALID_NUM_SEAT));
        return 0;
    }

    // Get number of preferred seats specified
    for (int i = 0; request->preferred_seats[i] != -1; i++) {
        num_preferred_seats++;
    }
    // Check if number of preferred seats is valid
    if(num_preferred_seats < request->num_wanted_seats){
        fprintf(stderr, "[TICKET OFFICE %ld]: Number of preferred seats (%d) smaller than number of wanted seats (%d)\n", tid, num_preferred_seats, request->num_wanted_seats);
        write(fd, INVALID_NUM_PREF_SEAT, sizeof(INVALID_NUM_PREF_SEAT));
        return 0;
    }
    if(num_preferred_seats > MAX_CLI_SEATS){
        fprintf(stderr, "[TICKET OFFICE %ld]: Number of preferred seats bigger than permited (%d)\n", tid, MAX_CLI_SEATS-1);
        write(fd, INVALID_NUM_PREF_SEAT, sizeof(INVALID_NUM_PREF_SEAT));
        return 0;
    }

    // Check if preferred seats are valid
    for (int i = 0; i < num_preferred_seats; i++) {
        if(request->preferred_seats[i] >= num_room_seats || request->preferred_seats[i] < 0){
            fprintf(stderr, "[TICKET OFFICE %ld]: Preferred seat %d do not exist\n", tid, request->preferred_seats[i] + 1);
            write(fd, INVALID_SEAT_ID, sizeof(INVALID_SEAT_ID));
            return 0;
        }
    }

    return 1;
}

int isSeatFree(int *seats, int seatNum){
    if(seats[seatNum])
        return 0;
    else
        return 1;
}

void bookSeat(int *seats, int seatNum, int clientId){
    seats[seatNum] = clientId;
}

void freeSeat(int *seats, int seatNum){
    seats[seatNum] = 0;
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
