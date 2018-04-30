#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>

#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[]) {
  printf("** Running process %d (PGID %d) **\n", getpid(), getpgrp());

	int   fd;
    char  str[100];

	int  fd2, messagelen, i;
	char message[200];


  if (argc != 3) {
  	printf("It sgould be passed to the client three arguments.");

  } else {
    printf("ARGS: %s | %s\n", argv[1], argv[2]);
	}

	// pid_t pid = getpid();

	// char  fifoName[10] = "ans%0", &pid;
	
 //    mkfifo("/tmp/myfifo",0660);
 //    fd=open("/tmp/myfifo",O_RDONLY);

 //    while(readline(fd,str)) printf("%s",str);
    // close(fd);


    	do {
        	fd=open("requests", O_WRONLY);
        	if (fd==-1) sleep(1);
    	} while (fd==-1);


        sprintf(message, "%d\n", getpid());
        messagelen = strlen(message)+1;
        write(fd, message, messagelen);

        message[0] = '\0';

		sprintf(message,"%d\n", argv[1]);
        messagelen = strlen(message)+1;
        write(fd, message, messagelen);

  //       sprintf(message,"Hello no. %d from process %d\n", i, getpid());
  //       messagelen=strlen(message)+1;
  //       write(fd,message,messagelen);

        // sleep(3);
        // char word[256];

        // fgets(word, sizeof(word), stdin);
        // write(fd,word,strlen(word));
    
	

  sleep(1);

  return 0;
}


// Processo para realizar a requisição
void func() {

}