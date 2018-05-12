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

// Defined constants
#define MAX_MSG_LEN 100                 // Maximum message lenght
#define MAX_CLI_SEATS 10                // Maximum number of seats
#define SV_LOG_NAME "slog.txt"          // Server logging file name
#define SV_BOOK_NAME "sbook.txt"        // Server bookings file name

// Defined Error constants
#define INVALID_NUM_SEAT         "-1"   // Number of desired seats is higher than permited
#define INVALID_NUM_PREF_SEAT    "-2"   // Number of preferred seats is invalid
#define INVALID_SEAT_ID          "-3"   // ID of preferred seat is invalid
#define PARAM_ERROR              "-4"   // Any other parameter error
#define BOOKING_FAILED           "-5"   // Could not reservate all the desired seats
#define FULL_ROOM                "-6"   // The room is full

// Defined function-like macros
#define DELAY(usec)       usleep(usec)  // Simulate the existence of some delay

// Global variables
int num_room_seats;                     // Number of seats
int num_tickets_offices;                // Number of ticket offices (threads)
unsigned long int open_time;            // ticket offices open time (seconds)
int *seats;                             // Seats
int keep_going;                         // Alarm flag
int sv_log_fd;                          // Server logging file file descriptor
int sv_book_fd;                         // Server bookings file file descriptor
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
static void cleanup_handler(void *arg);                 // Cleanup Handler executed when thread is canceled


// MAIN
int main(int argc, char const *argv[]) {
    int fd,                             // File Descriptor
        *tnum,                          // Thread numbers
        len;                            // Lenght of bkmsg
    char request[MAX_MSG_LEN+1],        // string formatted request
         *args,                         // parameters as a string
         *bkmsg;                        // Message to write in server bookings file
    requests *rq;                       // structured formatted request
    pthread_t *tids;                    // Thread ids
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
    args = (char*)malloc(sizeof(argv[0]));
    strcpy(args, argv[0]);
    for (int i = 1; i < argc; i++) {
        args = (char*)realloc(args, sizeof(args) + sizeof(argv[i]));
        strcat(args, " ");
        strcat(args, argv[i]);
    }

    // Test args agains regex
    if (regexec(&reg, args, 0, NULL, 0)) {
        fprintf(stderr, "Usage: server <num_room_seats> <num_tickets_offices> <open_time>\n");
        exit(1);
    }
    regfree(&reg);

    printf("[MAIN]: Setting global parameters\n");
    // Set global variables
    num_room_seats = strtol(argv[1], NULL, 10);
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

    // Allocate memory for thread ids and numbers
    tids = (pthread_t*)malloc(num_tickets_offices * sizeof(pthread_t));
    tnum = (int*)malloc(num_tickets_offices * sizeof(int));

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

    // Create num_tickets_offices threads
    printf("[MAIN]: Creating %d ticket offices\n", num_tickets_offices);
    for(int i = 0; i < num_tickets_offices; i++){
        tnum[i] = i+1;
        if(pthread_create(&tids[i], NULL, requestHandler, &tnum[i])){
            printf("[MAIN]: Error creating thread %d\n", i+1);
            exit(1);
        }
    }
    printf("[MAIN]: Created %d ticket offices\n", num_tickets_offices);

    // Estrablish SIGALRM handler
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
        if(read(fd, request, MAX_MSG_LEN) > 0){
            printf("received request: %s\n", request);
            rq = requestDisassembler(request);
            //requestHandler(rq);
            rq_buffer = rq;
            pthread_cond_broadcast(&cvar);
        }
    }

    // Close FIFO 'requests'
    printf("[MAIN]: Closing FIFO 'requests'\n");
    close(fd);
    printf("[MAIN]: FIFO 'requests' closed\n");

    // Destroy FIFO 'requests'
    printf("[MAIN]: Destroying FIFO 'requests'\n");
    if(unlink("requests") < 0)
        fprintf(stderr, "[MAIN]: Error destroying FIFO 'requests'\n");
    else
        printf("[MAIN]: Destroyied FIFO 'requests'\n");

    // Terminate threads
    printf("[MAIN]: Terminating %d ticket offices\n", num_tickets_offices);
    for (int i = 0; i < num_tickets_offices; i++) {
        pthread_cancel(tids[i]);
        // pthread_mutex_unlock(&rqt_mut);
        pthread_join(tids[i], NULL);
    }
    printf("[MAIN]: Terminated %d ticket offices\n", num_tickets_offices);


    printf("[MAIN]: Writing to server booking file\n");
    if((sv_book_fd = open(SV_BOOK_NAME, O_WRONLY | O_TRUNC | O_CREAT, 0660)) < 0){
        fprintf(stderr, "[MAIN]: Error opening server booking file\n");
        exit(1);
    }
    bkmsg = malloc(MAX_MSG_LEN * sizeof(char));
    len = 0;
    for (int i = 0; i < num_room_seats; i++) {
        if(seats[i]){
            len += sprintf(bkmsg+len, "%04d\n", i+1);
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
    requests *rq;
    int err = 0,
        fd,
        num_preferred_seats,
        num_wanted_seats,
        *preferred_seats,
        tnum = *(int*)tid,
        attempts = 3,
        len = 0;
    char *fifo,
         msg[MAX_MSG_LEN],
         *logmsg;

    printf("[TICKET OFFICE %d]: Ready\n", tnum);

    // Set Cleanup Handler
    pthread_cleanup_push(cleanup_handler, &tnum);

    logmsg = malloc(MAX_MSG_LEN * sizeof(char));

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
        len = sprintf(logmsg, "%02d-%d-%02d:", tnum, rq->client, rq->num_wanted_seats);
        for (int i = 0; rq->preferred_seats[i] != INT_MIN; i++) {
            len += sprintf(logmsg + len, " %04d", rq->preferred_seats[i] + 1);
        }
        len += sprintf(logmsg+len, " -");

        // Handle request
        if((err = validateRequest(seats, rq)) == 0){
            printf("[TIKEC OFFICE %d]: Handling request\n", tnum);
            num_preferred_seats = 0;
            num_wanted_seats = rq->num_wanted_seats;
            preferred_seats = malloc(sizeof(rq->preferred_seats));

            for (int i = 0; rq->preferred_seats[i] != INT_MIN; i++) {
                num_preferred_seats++;
                preferred_seats[i] = rq->preferred_seats[i];
            }


            // Reservate seats
            for (int i = 0; i < num_preferred_seats && num_wanted_seats; i++) {
                pthread_mutex_lock(&seats_mut);
                if(isSeatFree(seats, rq->preferred_seats[i])){
                    bookSeat(seats, rq->preferred_seats[i], rq->client);
                    printf("[TICKET OFFICE %d]: Seat %d booked\n", tnum, rq->preferred_seats[i] + 1);
                    num_wanted_seats--;
                }
                else{
                    printf("[TICKET OFFICE %d]: Seat %d already taken\n", tnum, rq->preferred_seats[i] + 1);
                    preferred_seats[i] = -1;
                }
                pthread_mutex_unlock(&seats_mut);
            }


            if(num_wanted_seats){   // Could not reservate the total number of desired seats
                printf("[TICKET OFFICE %d]: Could not book the total number of seats desired. Rolling back booking operation\n", tnum);
                write(fd, BOOKING_FAILED, sizeof(BOOKING_FAILED));

                // Rollback operation
                pthread_mutex_lock(&seats_mut);
                for (int i = 0; i < num_preferred_seats; i++) {
                    if (preferred_seats[i] != -1) {
                        freeSeat(seats, rq->preferred_seats[i]);
                    }
                }
                len += sprintf(logmsg+len, " NAV\n");
                pthread_mutex_unlock(&seats_mut);
            }
            else{   // successfuly reservated all desired seats
                printf("[TICKET OFFICE %d]: successfuly booked seats", tnum);
                sprintf(msg, "%d", rq->num_wanted_seats);

                for (int i = 0, j = 1; i < num_preferred_seats; i++, j+=2) {
                    if (preferred_seats[i] != -1) {
                        printf(" %d", rq->preferred_seats[i] + 1);

                        sprintf(&msg[j], " %d", rq->preferred_seats[i] + 1);
                        len += sprintf(logmsg+len, " %04d", rq->preferred_seats[i] + 1);
                    }
                }
                len += sprintf(logmsg+len, "\n");

                write(fd, msg, sizeof(msg));
            }

            printf("\n");
        }
        else if(err == -1){
            // Number of wanted seats higher than permited
            write(fd, INVALID_NUM_SEAT, sizeof(INVALID_NUM_SEAT));
            fprintf(stderr, "[TICKET OFFICE %d]: Number of seats wanted higher than permited\n", tnum);
            len += sprintf(logmsg+len, " MAX\n");
        }
        else if(err == -2){
            // Number of preferred seats invalid
            write(fd, INVALID_NUM_PREF_SEAT, sizeof(INVALID_NUM_PREF_SEAT));
            fprintf(stderr, "[TICKET OFFICE %d]: Number of preferred seats is invalid\n", tnum);
            len += sprintf(logmsg+len, " NST\n");
        }
        else if(err == -3){
            // One or more of the preferred seats id is invalid
            write(fd, INVALID_SEAT_ID, sizeof(INVALID_SEAT_ID));
            fprintf(stderr, "[TICKET OFFICE %d]: One of more of the preferred seats id is invalid\n", tnum);
            len += sprintf(logmsg+len, " IID\n");
        }
        else if(err == -4){
            // Invalid parameters
            write(fd, PARAM_ERROR, sizeof(PARAM_ERROR));
            fprintf(stderr, "[TICKET OFFICE %d]: Invalid parameters\n", tnum);
            len += sprintf(logmsg+len, " ERR\n");
        }
        else if(err == -6){
            // Room is full
            write(fd, FULL_ROOM, sizeof(FULL_ROOM));
            fprintf(stderr, "[TICKET OFFICE %d]: Room is full\n", tnum);
            len += sprintf(logmsg+len, " FUL\n");
        }

        write(sv_log_fd, logmsg, len);
    }

    len = sprintf(logmsg, "%d-CLOSE\n", tnum);
    write(sv_log_fd, logmsg, len);

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
    if(seats[seatNum]){
        return 0;
    }
    else{
        return 1;
    }
}

// SEAT BOOKER
void bookSeat(int *seats, int seatNum, int clientId){
    seats[seatNum] = clientId;
}

// SEAT UNBOOKER
void freeSeat(int *seats, int seatNum){
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
    char* fifo = (char*)malloc(strlen("ans") + get_number_lenght(pid) + 1);

    sprintf(fifo, "ans%d", pid);

    return fifo;
}

// SIGNAL SIGALRM HANDLER
void alarmHandler(int signum){
    keep_going = 0;
    signum = signum;
    printf("[MAIN]: Closing time!\n");
}

// THREAD CANCELATION HANDLER
static void cleanup_handler(void *tnum){
    char *logmsg;
    int len;

    pthread_mutex_unlock(&seats_mut);
    pthread_mutex_unlock(&rqt_mut);

    logmsg = malloc(MAX_MSG_LEN * sizeof(char));

    len = sprintf(logmsg, "%d-CLOSE\n", *(int*)tnum);

    write(sv_log_fd, logmsg, len);
}
