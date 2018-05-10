#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#define WIDTH_PID "5"
#define WIDTH_XX "2"
#define WIDTH_NN "2"
#define WIDTH_SEAT "4"

void postRequest(char *argv[], pid_t pid);
void getResponse(int timeout, pid_t pid);
int readline(int fd, char *str);
void parseResponse(char *response, pid_t pid);
void writeLog(pid_t pid, int reservedSeats, char *seats, char* ids);


int main(int argc, char *argv[]) {
    printf("** Running process %d (PGID %d) **\n", getpid(), getpgrp());
    
    pid_t pids = getpid();
    pid_t pid;
    int timeout = 0;
    
    // int messagelen;
    // char message[200];
    // char str[200];
    
    
    if (argc != 4) {
        printf("It should be passed to the client three arguments.\n");
    } else {
        printf("ARGS: %s | %s | %s\n", argv[1], argv[2], argv[3]);
    }
    timeout = atoi(argv[1]);
    
    pid = fork();
    if (pid == -1) {
        printf("Fatal error.");
        exit(0);
    } else if (pid == 0) {
        //        sleep(2);
        //        postRequest(argv, pid);
        
        getResponse(timeout, pids);
    } else {
        //        getResponse(timeout);
        
        sleep(2);
        postRequest(argv, pids);
    }
    
    sleep(1);
    
//
//    char message[50] = "7 8 9 10 11";
//    char message2[50] = "29 99 101 102 9103";
//
//    char message3[50] = "7 8 9 10 11";
//    char message4[50] = "29 99 101 102 9103";
//    writeLog(pids, 5, message, message2);
//
//    writeLog(pids, 3, message3, message4);
//    writeLog(pids, -1, NULL, NULL);
//    writeLog(pids, -2, NULL, NULL);
//    writeLog(pids, -3, NULL, NULL);
//    writeLog(pids, -4, NULL, NULL);
//    writeLog(pids, -5, NULL, NULL);
//    writeLog(pids, -6, NULL, NULL);
    
    
    
    sleep(1);
    return 0;
}

/**
 Parameters
 pid: process pid
 reservedSeats: The error id in case reservedSeats < 0 and the number of reserved seats in case reservedSeats > 0
 seats: The string with all the reserved seats.
 */
void writeLog(pid_t pid, int reservedSeats, char *seats, char* ids) {
    
    int fd, fdbook;
    char messageId[10];
    char message[200];
    int seatInt;
    //    int idInt;
    char *currSeat;
    char *currId;
    
    if ((fd = open("clog.txt", O_WRONLY | O_APPEND | O_CREAT, 0644)) < 0) {
        perror("Logfile");
        exit(1);
    }
    if ((fdbook = open("cbook.txt", O_WRONLY | O_APPEND | O_CREAT, 0644)) < 0) {
        perror("Book");
        exit(1);
    }
    
    
    if (reservedSeats > 0) {
        int *arrayIds = (int *)malloc(reservedSeats * sizeof(int));
        currId = strtok(ids, " ");
        for (int i = 0; i < reservedSeats; i++) {
            arrayIds[i] = atoi(currId);
            currId = strtok(NULL, " ");
        }
        
        currSeat = strtok(seats, " ");
        for (int i = 0; i < reservedSeats; i++) {
            
            snprintf(message, sizeof message, "%0"WIDTH_PID"d %0"WIDTH_XX"d.%0"WIDTH_NN"d %0"WIDTH_SEAT"d\n", pid, atoi(currSeat), reservedSeats, *arrayIds++);
            
            write(fd, message, strlen(message));
            
            snprintf(messageId, sizeof messageId, "%0"WIDTH_SEAT"d\n", *arrayIds);
            write(fdbook, messageId, strlen (messageId));
            
            currSeat = strtok(NULL, " ");
        }
        
    } else {
        char errorMessage[4];
        if (reservedSeats == -1) {
            sprintf(errorMessage, "MAX");
        } else if (reservedSeats == -2) {
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
        
        snprintf(message, sizeof message, "%05d %s", pid, errorMessage);
        strcat(message, "\n");
        
        write(fd, message, strlen(message));
    }
    
    //    write(fd, message, strlen(message));
    close(fd);
    
}

void getResponse(int timeout, pid_t pid) {
    char  fifoName[10];
    int fdAnswers;
    char response[200];
    time_t base = time (0);
    
    sprintf(fifoName, "ans%d", pid);
    
    mkfifo(fifoName, 0660);
    fdAnswers = open(fifoName, O_RDONLY);
    
    while(readline(fdAnswers, response) && ((time(0) - base) < (timeout+2))) {
        printf("RESPONSE: %s", response);
        parseResponse(response, pid);
    }
    
    close(fdAnswers);
    printf("ended timeout.");
    exit(0);
}

void parseResponse(char *response, pid_t pid) {
    char *part;
    int id;
    int aux;
    
    part = strtok (response, " ");
    id = atoi(part);
    
    printf("RESPONSE BEFORE PARSE: %s", response);
    
    if (id < 0) {
//        writeLog(pid, <#int reservedSeats#>, <#char *seat#>, <#char *id#>)
        
        printf("\n");
        if (id == -1) {
            part = strtok(NULL, " ");
            aux = atoi(part);
    
            printf("The number of desired seats is greater than the max allowed. (%d)", aux);
        } else if (id == -2) {
            printf("The number of id's of the desired seats aren't valid.");
        } else if (id == -3) {
            printf("The id of the desired seats aren't valid.");
        } else if (id == -4) {
            printf("Error in the parameters.");
        } else if (id == -5) {
            printf("At least one of the desired seats is not available.");
        } else if (id == -6) {
            printf("The room is full.");
        }
    } else {
//        part = strtok(NULL, " ");
        aux = atoi(part);
        printf("%d", aux);
    }
}


// Processo para realizar a requisição
void postRequest(char *argv[], pid_t pid) {
    int fdRequest;
    char message[200];
    int messagelen;
    char *part;
    
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
    
    exit(0);
}

int readline(int fd, char *str)
{
    int n;
    do {
        n = read(fd,str,1);
    } while (n>0 && *str++ != '\0');
    return (n>0);
}
