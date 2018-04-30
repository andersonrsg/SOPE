#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

void postRequest(char *argv[]);

int main(int argc, char *argv[]) {
    printf("** Running process %d (PGID %d) **\n", getpid(), getpgrp());
    
    int   fdAnswers;
    pid_t pid = getpid();
    
    int messagelen, i;
    char message[200];
    char str[200];
    
    
    if (argc != 3) {
        printf("It should be passed to the client three arguments.\n");
        
    } else {
        printf("ARGS: %s | %s\n", argv[1], argv[2]);
    }
    
    
    postRequest(argv);
    
    sleep(1);
    
    getResponse();
    
    
    
    
    // sleep(3);
    // char word[256];
    
    // fgets(word, sizeof(word), stdin);
    // write(fd,word,strlen(word));
    
    
    
    sleep(1);
    
    return 0;
}


void getResponse() {
	char  fifoName[10];
    sprintf(fifoName, "ans%d", getpid());
    
    
    mkfifo(fifoName, 0660);
    fdAnswers = open(fifoName, O_RDONLY);
    
    while(readline(fdAnswers,str)) printf("%s", str);
    close(fdAnswers);
}


// Processo para realizar a requisição
void postRequest(char *argv[]) {
    int fdRequest;
    char message[200];
    int messagelen;
    
    do {
        fdRequest = open("requests", O_WRONLY);
        if (fdRequest == -1) sleep(1);
    } while (fdRequest == -1);
    
    message[0] = '\0';
    
    
    sprintf(message, "%d\n", getpid());
    messagelen = strlen(message) + 1;
    write(fdRequest, message, messagelen);
    message[0] = '\0';
    
    sprintf(message, "%s\n", argv[1]);
    messagelen = strlen(message) + 1;
    write(fdRequest, message, messagelen);
    message[0] = '\0';
    
    sprintf(message, "%s", argv[2]);
    messagelen = strlen(message)+1;
    write(fdRequest, message, messagelen);    
}
