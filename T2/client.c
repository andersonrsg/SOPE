#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <signal.h>
#include "constants.h"

// Uncomment the following line to print additional information for better debbuging.
#define DEBUG_MODE

#define WIDTH_PID "5"
#define WIDTH_XX "2"
#define WIDTH_NN "2"
#define WIDTH_SEAT "4"

struct response {
    int timeout;
    int pid;
};

void *postRequest(char *argv[], pid_t pid, int timeout);
void *getResponse(void *arg);
int readline(int fd, char *str);
void parseResponse(char *response, pid_t pid);
void writeLog(pid_t pid, int reservedSeats, char *seats);
void alarmHandler(int signum);
char loading(char curr);

int timeNotOver = 1;
int signums = 0;

int main(int argc, char *argv[]) {
    pid_t pids = getpid();
    
    int timeout = 0;
    pthread_t tresponse;
    struct response resp;
    
//    char currChar = '|';
    
//    printf("** Running process %d (PGID %d) **\n", pids, getpgrp());
//    while (1) {
//        printf("\r%s", &currChar);
//        currChar = loading(currChar);
//        sleep(1);
//    }
    
    if (argc != 4) {
        printf("It should be passed to the client three arguments.\n");
    } else {
        printf("ARGS: %s | %s | %s\n", argv[1], argv[2], argv[3]);
    }
    printf("[CLIENT]: Client started with id %d\n", pids);
    
    timeout = atoi(argv[1]);

    resp.timeout = timeout;
    resp.pid = pids;
    if(pthread_create(&tresponse, NULL, getResponse, &resp)){
        printf("[CLIENT %d]: Failed to wait for response\n", pids);
        exit(1);
    }
    
    postRequest(argv, pids, timeout);

    
    pthread_join(tresponse, NULL);
    printf("[CLIENT %d]: Finished execution\n", pids);
    return 0;
}

/**
 Parameters
 pid: process pid
 reservedSeats: The error id in case reservedSeats < 0 and the number of reserved seats in case reservedSeats > 0
 seats: The string with all the reserved seats.
 */
void writeLog(pid_t pid, int reservedSeats, char *seats) {
    
    int fd, fdbook;
    char messageId[10];
    char message[200];
    char *currSeat;

    if ((fd = open("clog.txt", O_WRONLY | O_APPEND | O_CREAT, 0644)) < 0) {
        perror("Logfile");
        exit(1);
    }
    if ((fdbook = open("cbook.txt", O_WRONLY | O_APPEND | O_CREAT, 0644)) < 0) {
        perror("Book");
        exit(1);
    }

#ifdef DEBUG_MODE
    printf("reserved %d\n",reservedSeats);
    printf("seats %s\n", seats);
#endif
    
    if (reservedSeats > 0) {
        
        currSeat = strtok(seats, " ");
        for (int i = 0; i < reservedSeats; i++) {
            currSeat = strtok(NULL, " ");
            
            int currSeatInt = atoi(currSeat);
            
            snprintf(message, sizeof message, "%0"WIDTH_PID"d %0"WIDTH_XX"d.%0"WIDTH_NN"d %0"WIDTH_SEAT"d\n", pid, i+1, reservedSeats, currSeatInt);
            write(fd, message, strlen(message));
            
            snprintf(messageId, sizeof messageId, "%0"WIDTH_SEAT"d\n", currSeatInt);
            write(fdbook, messageId, strlen (messageId));
        
        }
        
    } else {
        
        char errorMessage[4];
        if (reservedSeats == MAX) {
            sprintf(errorMessage, "MAX");
        } else if (reservedSeats == NST) {
            sprintf(errorMessage, "NST");
        } else if (reservedSeats == -3) {
            sprintf(errorMessage, "IID");
        } else if (reservedSeats == -4) {
            sprintf(errorMessage, "ERR");
        } else if (reservedSeats == -5) {
            sprintf(errorMessage, "NAV");
        } else if (reservedSeats == -6) {
            sprintf(errorMessage, "FUL");
        }
    
        snprintf(message, sizeof message, "%05d %s\n", pid, errorMessage);
        write(fd, message, strlen(message));
    }
    
//    close(fd);
}

//void getResponse(int timeout, pid_t pid) {
void *getResponse(void *arg) {
    
    struct response *param = (struct response*) arg;
    
    char fifoName[10];
    int fdAnswers;
    char response[200];
    
    sprintf(fifoName, "ans%d", param->pid);
    
    mkfifo(fifoName, 0660);
    fdAnswers = open(fifoName, O_RDONLY);
    
    while(readline(fdAnswers, response) && timeNotOver == 1) {
#ifdef DEBUG_MODE
        printf("RESPONSE: %s\n", response);
#endif
        parseResponse(response, param->pid);
    }
    
    close(fdAnswers);
    printf("[CLIENT %d]: Ended execution\n", param->pid);
//    unlink(fifoName);
    exit(0);
}

void parseResponse(char *response, pid_t pid) {
    char *idString;
    char *responsedup = strdup(response);
    int id;

    printf("[CLIENT %d]: Received response %s\n", pid, response);
    sleep(1);
    
    idString = strtok (response, " ");
    id = atoi(idString);
    if (id == 0) {
        printf("[CLIENT %d]: Error in server response\n", pid);
    } else if (id < 0) {

        if (id == -1) {
            idString = strtok(NULL, " ");

            printf("[CLIENT %d]: The number of desired seats is greater than the max allowed. (%d)\n", pid, atoi(idString));
        } else if (id == -2) {
            printf("[CLIENT %d]: The number of id's of the desired seats aren't valid.\n", pid);
        } else if (id == -3) {
            printf("[CLIENT %d]: The id of the desired seats aren't valid.\n", pid);
        } else if (id == -4) {
            printf("[CLIENT %d]: Error in the parameters.\n", pid);
        } else if (id == -5) {
            printf("[CLIENT %d]: At least one of the desired seats is not available.\n", pid);
        } else if (id == -6) {
            printf("[CLIENT %d]: The room is full.\n", pid );
        }
        
        writeLog(pid, id, NULL);
        
        exit(0);
    } else {
        
#ifdef DEBUG_MODE
        printf("[CLIENT %d]: received response: %d\n", pid, id);
#endif
        writeLog(pid, id, responsedup);
        
#ifdef DEBUG_MODE
        printf("[CLIENT %d]: Write log working properly\n", pid);
#endif
    }
}


// Processo para realizar a requisição
void *postRequest(char *argv[], pid_t pid, int timeout) {
    sleep(3);
    int fdRequest;
    char message[200];
    unsigned long messagelen;
    
    do {
        fdRequest = open("requests", O_WRONLY);
        if (fdRequest == -1) sleep(1);
    } while (fdRequest == -1);
    
    
    sprintf(message, "%d ", pid);
    strcat(message, argv[2]);
    strcat(message, " ");
    strcat(message, argv[3]);
    
    messagelen = strlen(message) + 1;
    
    write(fdRequest, message, messagelen);
    printf("[CLIENT %d]: successfully sent request\n", pid);
    printf("[CLIENT %d]: waiting for response\n", pid);
    printf("[CLIENT %d]: max wait time: %d seconds\n", pid, timeout);
    signal(SIGALRM, alarmHandler);
    
    // Set alarm to signal after <timeout> seconds
    if(alarm(timeout)){
        exit(1);
    }
    
    return NULL;
}

char loading(char curr) {
    switch (curr) {
        case '|':
            return '/';
            break;
        case '/':
            return '-';
            break;
        case '-':
            return '\\';
            break;
        case '\\':
            return '|';
            break;
            
        default:
            break;
    }
    return '|';
}

int readline(int fd, char *str)
{
    int n;
//    char currChar = '|';
    do {
        n = read(fd,str,1);
        
    } while ((n>0 && *str++ != '\0') && timeNotOver == 1);
    return (n>0);
}

void alarmHandler(int signum) {
    timeNotOver = 0;
    signums = signum;
    printf("[CLIENT %d]: Client timeout!\n", getpid());
    exit(0);
}
