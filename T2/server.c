// standard C libraries
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

// UNIX/Linux libraries
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <regex.h>
#include "constants.h"

// Defined function-like macros
#define DELAY(sec)     sleep(sec)       // Simulate the existence of some delay

// Global variables
int num_room_seats;                     // Number of seats
int num_tickets_offices;                // Number of ticket offices (threads)
unsigned long int open_time;            // ticket offices open time (seconds)
int *seats;                             // Seats
int keep_going;                         // Alarm flag
int sv_log_fd;                          // Server logging file file descriptor
int sv_book_fd;                         // Server bookings file file descriptor
int rqt_fifo_fd;                        // FIFO requests file descriptor
pthread_t *tids;                        // Threads' id
typedef struct{
    int client;                         // store the PID of the client
    int num_wanted_seats;               // store the number of wanted seats
    int *preferred_seats;               // store a list of preferred seats
} requests;
requests *rq_buffer;                     // Shared buffer
pthread_mutex_t rqt_mut = PTHREAD_MUTEX_INITIALIZER;    // Initialize the request mutex
pthread_mutex_t seats_mut = PTHREAD_MUTEX_INITIALIZER;  // Initialize the seats mutex
pthread_cond_t cvar = PTHREAD_COND_INITIALIZER;         // Initialize the conditional variable

// Functions
int get_number_lenght(size_t number);                   // Used to get lenght of the number
char* getClientFIFO(int pid);                           // Used to get the name of the client FIFO
requests* requestDisassembler(char* request);           // Used to build a requests struct from a request string
void *requestHandler(void *arg);                        // Thread function used to handle the request
int validateRequest(int *seats, requests *request);     // Used to validate a request
int isSeatFree(int *seats, int seatNum);                // Used to check if seat is free
void bookSeat(int *seats, int seatNum, int clientId);   // Used to book a seat
void freeSeat(int *seats, int seatNum);                 // Used to free a seat
void alarmHandler(int signum);                          // Used to signal the main thread to stop recieving requests
void sigintHandler(int signum);                         // Interrupt signal handler
static void cleanup_handler(void *arg);                 // Cleanup Handler executed when thread is canceled


// MAIN
int main(int argc, char const *argv[]) {
    int *tnum,                          // Thread numbers
        len;                            // Lenght of bkmsg
    char request[MAX_MSG_LEN+1],        // string formatted request
         *args,                         // parameters as a string
         *bkmsg;                        // Message to write in server bookings file
    requests *rq;                       // structured formatted request
    regex_t reg;                        // regex expression

    // Usage
    if(argc != 4){
        fprintf(stderr, "Usage: server <num_room_seats> <num_tickets_offices> <open_time>\n");
        exit(1);
    }

    // Compile regex
    if (regcomp(&reg, "\\.\\/server( +[0-9]+){3} *", REG_EXTENDED)){
        fprintf(stderr, "[MAIN]: Error compiling regex\n");
        regfree(&reg);
        exit(1);
    }

    // Transform argv to string
    args = malloc(sizeof(argv[0]) + 1);
    strcpy(args, argv[0]);
    for (int i = 1; i < argc; i++) {
        args = realloc(args, sizeof(args) + sizeof(argv[i]) + 1);
        strcat(args, " ");
        strcat(args, argv[i]);
    }

    // Test args agains regex
    if (regexec(&reg, args, 0, NULL, 0)) {
        fprintf(stderr, "Usage: server <num_room_seats> <num_tickets_offices> <open_time>\n");
        exit(1);
    }
    regfree(&reg);
    free(args);

    // Set global variables
    printf("[MAIN]: Setting global parameters\n");
    if((num_room_seats = strtol(argv[1], NULL, 10)) > MAX_ROOM_SEATS){
        printf("[MAIN]: Number of room seats higher than permited\n");
        exit(1);
    }
    num_tickets_offices = strtol(argv[2], NULL, 10);
    open_time = strtol(argv[3], NULL, 10);
    keep_going = 1;
    seats = malloc(num_room_seats * sizeof(int));
    memset(seats, 0, num_room_seats * sizeof(int));
    rq_buffer = NULL;
    printf("[MAIN]: Global parameters setted\n");

    // Open or create server logging file
    printf("[MAIN]: Opening server logging file\n");
    if((sv_log_fd = open(SV_LOG_NAME, O_WRONLY | O_APPEND | O_CREAT, 0660)) < 0){
        printf("[MAIN]: Error opening server logging file\n");
        exit(1);
    }
    printf("[MAIN]: Opened server logging file\n");

    // Make FIFO 'ŕequests'
    printf("[MAIN]: Attempting to create FIFO '%s'\n", RQT_FIFO_NAME);
    if(mkfifo(RQT_FIFO_NAME, 0660) == 0){
        printf("[MAIN]: FIFO '%s' created successfuly\n", RQT_FIFO_NAME);
    }
    else{
        if(errno == EEXIST) printf("[MAIN]: FIFO '%s' already exists\n", RQT_FIFO_NAME);
        else {
            fprintf(stderr, "[MAIN]: Can't create FIFO '%s'\n", RQT_FIFO_NAME);
            exit(1);
        }
    }

    // Open FIFO 'requests' for reading and prevent waiting
    printf("[MAIN]: Attempting to open FIFO '%s'\n", RQT_FIFO_NAME);
    if((rqt_fifo_fd = open(RQT_FIFO_NAME, O_RDONLY | O_NONBLOCK)) == -1){
        fprintf(stderr, "[MAIN]: Error opening FIFO '%s' in READONLY mode", RQT_FIFO_NAME);
        exit(1);
    }
    printf("[MAIN]: successfuly opened FIFO '%s'\n", RQT_FIFO_NAME);

    // Allocate memory for thread ids and numbers
    tids = malloc(num_tickets_offices * sizeof(pthread_t));
    tnum = malloc(num_tickets_offices * sizeof(int));

    // Create num_tickets_offices threads
    printf("[MAIN]: Creating %d ticket offices\n", num_tickets_offices);
    for(int i = 0; i < num_tickets_offices; i++){
        tnum[i] = i+1;
        if(pthread_create(&tids[i], NULL, requestHandler, &tnum[i])){
            printf("[MAIN]: Error creating thread %d\n", i+1);
            exit(1);
        }
    }
    free(tnum);
    printf("[MAIN]: Created %d ticket offices\n", num_tickets_offices);

    // Establish SIGINT handler
    printf("[MAIN]: Establishing SIGINT handler\n");
    signal(SIGINT, sigintHandler);
    printf("[MAIN]: Established SIGINT handler\n");

    // Establish SIGALRM handler
    printf("[MAIN]: Establishing SIGALRM handler\n");
    signal(SIGALRM, alarmHandler);
    printf("[MAIN]: Established SIGALRM handler\n");

    // Set alarm to signal after <open_time> seconds
    printf("[MAIN]: Setting alarm to %ld seconds\n", open_time);
    if(alarm(open_time)){
        printf("[MAIN]: An alarm is already set and preventing the proper installation of a new alarm\n");
        exit(1);
    }
    printf("[MAIN]: Alarm setted\n");

    // Receive requests
    printf("[MAIN]: Ready to recieve requests\n");
    while (keep_going) {
        if(read(rqt_fifo_fd, request, MAX_MSG_LEN) > 0){
            printf("received request: %s\n", request);
            rq = requestDisassembler(request);
            rq_buffer = rq;
            pthread_cond_broadcast(&cvar);
        }
    }

    // Close FIFO 'requests'
    printf("[MAIN]: Closing FIFO '%s'\n", RQT_FIFO_NAME);
    if(close(rqt_fifo_fd) < 0){
        fprintf(stderr, "[MAIN]: Error closing FIFO '%s'\n", RQT_FIFO_NAME);
        exit(1);
    }
    else{
        printf("[MAIN]: FIFO '%s' closed\n", RQT_FIFO_NAME);
    }

    // Destroy FIFO 'requests'
    printf("[MAIN]: Destroying FIFO '%s'\n", RQT_FIFO_NAME);
    if(unlink(RQT_FIFO_NAME) < 0){
        fprintf(stderr, "[MAIN]: Error destroying FIFO '%s'\n", RQT_FIFO_NAME);
        exit(1);
    }
    else{
        printf("[MAIN]: Destroyed FIFO '%s'\n", RQT_FIFO_NAME);
    }

    // Terminate threads
    printf("[MAIN]: Terminating %d ticket offices\n", num_tickets_offices);
    for (int i = 0; i < num_tickets_offices; i++) {
        pthread_cancel(tids[i]);
        pthread_join(tids[i], NULL);
    }
    free(tids);
    printf("[MAIN]: Terminated %d ticket offices\n", num_tickets_offices);

    // Destroy mutexes
    printf("[MAIN]: Destroying Request mutex\n");
    if(pthread_mutex_destroy(&rqt_mut)){
        fprintf(stderr, "[MAIN]: Error destroying Request mutex\n");
        exit(1);
    }
    printf("[MAIN]: Destroyed Request mutex\n");

    printf("[MAIN]: Destroying Seats mutex\n");
    if(pthread_mutex_destroy(&seats_mut)){
        fprintf(stderr, "[MAIN]: Error destroying Seats mutex\n");
        exit(1);
    }
    printf("[MAIN]: Destroyed Seats mutex\n");


    printf("[MAIN]: Writing to server booking file\n");
    if((sv_book_fd = open(SV_BOOK_NAME, O_WRONLY | O_TRUNC | O_CREAT, 0660)) < 0){
        fprintf(stderr, "[MAIN]: Error opening server booking file\n");
        exit(1);
    }
    bkmsg = malloc((MAX_MSG_LEN + 1) * sizeof(char));
    len = 0;
    for (int i = 0; i < num_room_seats; i++) {
        if(seats[i]){
            len += sprintf(bkmsg+len, "%0"WIDTH_SEAT"d\n", i+1);
        }
    }
    write(sv_book_fd, bkmsg, len);
    printf("[MAIN]: Finished writing to server booking file\n");

    // Exit program successfuly
    printf("[MAIN]: Terminating server\n");
    write(sv_log_fd, "SERVER CLOSED\n", sizeof("SERVER CLOSED\n"));
    exit(0);
}

// REQUEST DISASSEMBLER
requests* requestDisassembler(char* request){
    requests *rq = malloc(sizeof(requests));
    char *token,
         *delim = " ";
    int i = 0;

    // get client PID
    printf("[REQUEST DISASSEMBLER]: Extracting Client PID from request\n");
    if((token = strtok(request, delim)) == NULL){
        fprintf(stderr, "[REQUEST DISASSEMBLER]: Error extracting Client PID from request\n");
        return NULL;
    }
    rq->client = strtol(token, NULL, 10);
    printf("[REQUEST DISASSEMBLER]: Extracted Client PID from request\n");

    printf("[REQUEST DISASSEMBLER]: Extracting Client desired number of seats from request\n");
    // get client desired number of seats
    if((token = strtok(NULL, delim)) == NULL){
        fprintf(stderr, "[REQUEST DISASSEMBLER]: Error extracting Client desired number of seats from request\n");
        return NULL;
    }
    rq->num_wanted_seats = strtol(token, NULL, 10);
    printf("[REQUEST DISASSEMBLER]: Extracted Client desired number of seats from request\n");

    printf("[REQUEST DISASSEMBLER]: Extracting Client preferred seats from request\n");
    // get the desired seats
    rq->preferred_seats = NULL;
    token = strtok(NULL, delim);
    while(token != NULL){
        rq->preferred_seats = realloc(rq->preferred_seats, ++i * sizeof(int));
        rq->preferred_seats[i-1] = strtol(token, NULL, 10) - 1;
        token = strtok(NULL, delim);
    }
    rq->preferred_seats = realloc(rq->preferred_seats, ++i * sizeof(int));
    rq->preferred_seats[i-1] = INT_MIN;
    printf("[REQUEST DISASSEMBLER]: extracted Client desired number of seats from request\n");

    printf("Cliente: %d\n", rq->client);
    printf("Num seats: %d\n", rq->num_wanted_seats);
    printf("desired seats: ");
    for(int j = 0; rq->preferred_seats[j] != INT_MIN; j++){
        printf("%d ", rq->preferred_seats[j] + 1);
    }
    printf("\n");

    return rq;
}

// REQUEST HANDLER
void *requestHandler(void *tid){
    requests *rq;                   // Store the request struct
    int err = 0,                    // Error value
        fd,                         // Client fifo file descriptor
        num_preferred_seats,        // Number of client preferred seats
        num_wanted_seats,           // Number of client desired seats
        num_booked_seats,           // NUumber of booked seats
        *booked_seats,              // Booked seats
        tnum = *(int*)tid,          // Thread number
        attempts = 3,               // Attempts to open client fifo
        log_len = 0,                // Lenght of logging message
        msg_len = 0;                // Lenght of success message
    char *fifo,                     // Client fifo name
         *msg,                      // Response message (in case of success)
         *logmsg;                   // Logging message

    printf("[TICKET OFFICE %d]: Ready\n", tnum);

    // Set Cleanup Handler
    pthread_cleanup_push(cleanup_handler, &tnum);

    msg = malloc((MAX_MSG_LEN + 1) * sizeof(char));
    logmsg = malloc((MAX_MSG_LEN + 1) * sizeof(char));

    sprintf(logmsg, "%d-OPEN\n", tnum);
    write(sv_log_fd, logmsg, sizeof(logmsg));


    while(1){
        pthread_mutex_lock(&rqt_mut);
        while(rq_buffer == NULL){
            pthread_cond_wait(&cvar, &rqt_mut);
        }

        printf("[TICKET OFFICE %d]: Request received\n",tnum );
        rq = rq_buffer;
        rq_buffer = NULL;
        pthread_mutex_unlock(&rqt_mut);

        // Disable cancellation until operation is done
        if(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL)){
            fprintf(stderr, "[TICKET OFFICE %d]: Error setting cancellation state. Ignoring request\n", tnum);
            continue;
        }

        // Get client dedicated fifo name
        fifo = malloc(sizeof("ans") + get_number_lenght(rq->client) + 1);
        if(sprintf(fifo, "ans%d", rq->client) < 0){
            printf("[TICKER OFFICE %d]: Error geting client %d FIFO name\n", tnum, rq->client);
            pthread_mutex_unlock(&rqt_mut);
            continue;
        }

        // Open client dedicated fifo (3 attempts)
        attempts  = 3;
        while (attempts--) {
            if((fd = open(fifo, O_WRONLY | O_NONBLOCK)) > 0){
                break;
            }
        }
        if(!attempts){
            printf("[TICKET OFFICE %d]: FIFO '%s' is not open for read\n", tnum, fifo);
            continue;
        }

        // Build log message
        log_len = sprintf(logmsg, "%0"WIDTH_THREAD"d-%0"WIDTH_PID"d-%0"WIDTH_NN"d:", tnum, rq->client, rq->num_wanted_seats);
        for (int i = 0; rq->preferred_seats[i] != INT_MIN; i++) {
            log_len += sprintf(logmsg + log_len, " %0"WIDTH_SEAT"d", rq->preferred_seats[i] + 1);
        }
        log_len += sprintf(logmsg+log_len, " -");

        // Handle request
        if((err = validateRequest(seats, rq)) == 0){
            printf("[TIKECT OFFICE %d]: Handling request\n", tnum);
            num_preferred_seats = 0;
            num_wanted_seats = rq->num_wanted_seats;
            booked_seats = NULL;
            num_booked_seats = 0;

            for (int i = 0; rq->preferred_seats[i] != INT_MIN; i++) {
                num_preferred_seats++;
            }


            // Reservate seats
            for (int i = 0; i < num_preferred_seats && num_wanted_seats; i++) {
                pthread_mutex_lock(&seats_mut);
                if(isSeatFree(seats, rq->preferred_seats[i])){
                    bookSeat(seats, rq->preferred_seats[i], rq->client);
                    printf("[TICKET OFFICE %d]: Seat %d booked\n", tnum, rq->preferred_seats[i] + 1);
                    num_wanted_seats--;
                    if((booked_seats = realloc(booked_seats, (++num_booked_seats) * sizeof(int))) == NULL){
                        fprintf(stderr, "[TICKET OFFICE %d]: Error storing booked seat %d\n", tnum, rq->preferred_seats[i] + 1);
                        continue;
                    }
                    booked_seats[num_booked_seats - 1] = rq->preferred_seats[i];
                }
                else{
                    printf("[TICKET OFFICE %d]: Seat %d already taken\n", tnum, rq->preferred_seats[i] + 1);
                }
                pthread_mutex_unlock(&seats_mut);
            }


            if(num_wanted_seats){   // Could not reservate the total number of desired seats
                printf("[TICKET OFFICE %d]: Could not book the total number of seats desired. Rolling back booking operation\n", tnum);
                msg_len = sprintf(msg, "%d", NAV);

                // Rollback operation
                pthread_mutex_lock(&seats_mut);
                for (int i = 0; i < num_booked_seats; i++) {
                    freeSeat(seats, booked_seats[i]);
                    printf("[TIKCET OFFICE %d]: Seat %d freed\n", tnum, booked_seats[i] + 1);
                }
                log_len += sprintf(logmsg+log_len, " NAV\n");
                pthread_mutex_unlock(&seats_mut);
            }
            else{   // successfuly reservated all desired seats
                printf("[TICKET OFFICE %d]: successfuly booked seats", tnum);
                msg_len = sprintf(msg, "%d", rq->num_wanted_seats);

                for (int i = 0; i < num_booked_seats; i++) {
                    printf(" %d", booked_seats[i] + 1);

                    msg_len += sprintf(msg + msg_len, " %d", booked_seats[i] + 1);
                    log_len += sprintf(logmsg+log_len, " %0"WIDTH_SEAT"d", booked_seats[i] + 1);
                }
                printf("\n");
                log_len += sprintf(logmsg+log_len, "\n");
            }

            printf("\n");
        }
        else if(err == -1){
            // Number of wanted seats higher than permited
            msg_len = sprintf(msg, "%d", MAX);
            fprintf(stderr, "[TICKET OFFICE %d]: Number of seats wanted higher than permited\n", tnum);
            log_len += sprintf(logmsg+log_len, " MAX\n");
        }
        else if(err == -2){
            // Number of preferred seats invalid
            msg_len = sprintf(msg, "%d", NST);
            fprintf(stderr, "[TICKET OFFICE %d]: Number of preferred seats is invalid\n", tnum);
            log_len += sprintf(logmsg+log_len, " NST\n");
        }
        else if(err == -3){
            // One or more of the preferred seats id is invalid
            msg_len = sprintf(msg, "%d", IID);
            fprintf(stderr, "[TICKET OFFICE %d]: One of more of the preferred seats id is invalid\n", tnum);
            log_len += sprintf(logmsg+log_len, " IID\n");
        }
        else if(err == -4){
            // Invalid parameters
            msg_len = sprintf(msg, "%d", ERR);
            fprintf(stderr, "[TICKET OFFICE %d]: Invalid parameters\n", tnum);
            log_len += sprintf(logmsg+log_len, " ERR\n");
        }
        else if(err == -6){
            // Room is full
            msg_len = sprintf(msg, "%d", FUL);
            fprintf(stderr, "[TICKET OFFICE %d]: Room is full\n", tnum);
            log_len += sprintf(logmsg+log_len, " FUL\n");
        }

        printf("[TICKET OFFICE %d]: Writing message '%s' of lenght %d\n\n", tnum, msg, msg_len);
        *(msg+msg_len) = '\0';
        msg_len++;
        write(fd, msg, msg_len);
        write(sv_log_fd, logmsg, log_len);

        // Enable cancellation
        if(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL)){
            fprintf(stderr, "[TICKET OFFICE %d]: Error enabling cancellation state. Exiting\n", tnum);
            pthread_exit(NULL);
        }
    }

    log_len = sprintf(logmsg, "%d-CLOSE\n", tnum);
    write(sv_log_fd, logmsg, log_len);

    pthread_cleanup_pop(0);

    pthread_exit(NULL);
}

// REQUEST VALIDATOR
int validateRequest(int *seats, requests *request){
    int room_full = 1,
        num_preferred_seats = 0;

    // Check if room is full
    for (int i = 0; i < num_room_seats; i++) {
        if(!seats[i]){
            room_full = 0;
        }
    }
    if(room_full){
        return -6;
    }

    // Check if client PID is valid
    if(request->client < 0){
        return -4;
    }

    // Check if desired number of seats if valid
    if(request->num_wanted_seats <= 0){
        return -4;
    }
    if(request->num_wanted_seats > MAX_CLI_SEATS){
        return -1;
    }

    // Get number of preferred seats specified
    // and check if any of them is invalid
    for (int i = 0; request->preferred_seats[i] != INT_MIN; i++) {
        num_preferred_seats++;
        if(request->preferred_seats[i] < 0){
            return -4;
        }
    }
    // Check if number of preferred seats is valid
    if(num_preferred_seats < request->num_wanted_seats){
        return -2;
    }
    if(num_preferred_seats > MAX_CLI_SEATS){
        return -2;
    }

    // Check if preferred seats are valid
    for (int i = 0; i < num_preferred_seats; i++) {
        if(request->preferred_seats[i] >= num_room_seats || request->preferred_seats[i] < 0){
            return -3;
        }
    }

    return 0;
}

// SEAT AVAILABILITY CHECK
int isSeatFree(int *seats, int seatNum){
    DELAY(1);
    if(seats[seatNum]){
        return 0;
    }
    else{
        return 1;
    }
}

// SEAT BOOKER
void bookSeat(int *seats, int seatNum, int clientId){
    DELAY(1);
    seats[seatNum] = clientId;
}

// SEAT UNBOOKER
void freeSeat(int *seats, int seatNum){
    DELAY(1);
    seats[seatNum] = 0;
}

//  NUMBER LENGHT
int get_number_lenght(size_t number){
    int i = 0;
    do {
        number /= 10;
        i++;
    } while(number > 0);
    return i;
}

// CLIENT FIFO ASSEMBLER
char* getClientFIFO(int pid){
    char* fifo = (char*)malloc(sizeof("ans") + get_number_lenght(pid) + 1);

    sprintf(fifo, "ans%d", pid);

    return fifo;
}

// SIGNAL SIGALRM HANDLER
void alarmHandler(int signum){
    (void)signum; // Ignore unused parameter
    keep_going = 0;
    printf("[MAIN]: Closing time!\n");
}

// SIGNAL SIGINT HANDLER
void sigintHandler(int signum){
    char answer,
        *bkmsg;
    int len;

    (void) signum;

    printf("\n\n[MAIN]: Do you really want to end executing? (Y/y for yes or any other key for no) ");
    scanf("%c%*[^\n]%*c", &answer);

    if(answer == 'Y' || answer == 'y'){
        // Close FIFO 'requests'
        printf("\n[MAIN]: Closing FIFO 'requests'\n");
        close(rqt_fifo_fd);
        printf("[MAIN]: FIFO 'requests' closed\n");

        // Destroy FIFO 'requests'
        printf("[MAIN]: Destroying FIFO 'requests'\n");
        if(unlink("requests") < 0)
            fprintf(stderr, "[MAIN]: Error destroying FIFO 'requests'\n");
        else
            printf("[MAIN]: Destroyed FIFO 'requests'\n");

        // Terminate threads
        printf("[MAIN]: Terminating %d ticket offices\n", num_tickets_offices);
        for (int i = 0; i < num_tickets_offices; i++) {
            pthread_cancel(tids[i]);
            pthread_join(tids[i], NULL);
        }
        free(tids);
        printf("[MAIN]: Terminated %d ticket offices\n", num_tickets_offices);

        // Destroy mutexes
        printf("[MAIN]: Destroying Request mutex\n");
        if(pthread_mutex_destroy(&rqt_mut)){
            fprintf(stderr, "[MAIN]: Error destroying Request mutex\n");
            exit(1);
        }
        printf("[MAIN]: Destroyed Request mutex\n");

        printf("[MAIN]: Destroying Seats mutex\n");
        if(pthread_mutex_destroy(&seats_mut)){
            fprintf(stderr, "[MAIN]: Error destroying Seats mutex\n");
            exit(1);
        }
        printf("[MAIN]: Destroyed Seats mutex\n");


        printf("[MAIN]: Writing to server booking file\n");
        if((sv_book_fd = open(SV_BOOK_NAME, O_WRONLY | O_TRUNC | O_CREAT, 0660)) < 0){
            fprintf(stderr, "[MAIN]: Error opening server booking file\n");
            exit(1);
        }
        bkmsg = malloc((MAX_MSG_LEN + 1) * sizeof(char));
        len = 0;
        for (int i = 0; i < num_room_seats; i++) {
            if(seats[i]){
                len += sprintf(bkmsg+len, "%0"WIDTH_SEAT"d\n", i+1);
            }
        }
        write(sv_book_fd, bkmsg, len);
        printf("[MAIN]: Finished writing to server booking file\n");

        // Exit program successfuly
        printf("[MAIN]: Terminating server\n");
        write(sv_log_fd, "SERVER INTERRUPTED\n", sizeof("SERVER INTERRUPTED\n"));
        exit(0);
    }
    else{
        raise(SIGCONT);
    }
}

// THREAD CANCELLATION HANDLER
static void cleanup_handler(void *tnum){
    char *logmsg;
    int len;

    pthread_mutex_unlock(&rqt_mut);

    logmsg = malloc((MAX_MSG_LEN + 1) * sizeof(char));

    len = sprintf(logmsg, "%d-CLOSE\n", *(int*)tnum);

    write(sv_log_fd, logmsg, len);
}
