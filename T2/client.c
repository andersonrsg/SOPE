#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>


void postRequest(char *argv[], pid_t pid);
void getResponse(int timeout);
int readline(int fd, char *str);
void parseResponse(char *response);

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
        
        getResponse(timeout);
    } else {
        //        getResponse(timeout);
        
        sleep(2);
        postRequest(argv, pids);
    }
    
    sleep(1);
    
    
    
    
    
    // sleep(3);
    // char word[256];
    
    // fgets(word, sizeof(word), stdin);
    // write(fd,word,strlen(word));
    
    
    
    sleep(1);
    return 0;
}


void getResponse(int timeout) {
    char  fifoName[10];
    int fdAnswers;
    char response[200];
    time_t base = time (0);
    
    sprintf(fifoName, "ans%d", getpid());
    
    mkfifo(fifoName, 0660);
    fdAnswers = open(fifoName, O_RDONLY);
    
    while(readline(fdAnswers, response) && ((time(0) - base) < (timeout+2))) {
        printf("%s", response);
        parseResponse(response);
    }
    
    close(fdAnswers);
    printf("ended timeout.");
}

void parseResponse(char *response) {
    char *part;
    int id;
    int aux;
    
    part = strtok (response, " ");
    id = atoi(part);
    
    if (id < 0) {
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
        part = strtok(NULL, " ");
        aux = atoi(part);
        
    }
}


// Processo para realizar a requisição
void postRequest(char *argv[], pid_t pid) {
    int fdRequest;
    char message[200];
    int messagelen;
    
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
}

int readline(int fd, char *str)
{
    int n;
    do {
        n = read(fd,str,1);
    } while (n>0 && *str++ != '\0');
    return (n>0);
}
