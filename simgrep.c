// simgrep.c
//
// Anderson Gralha - up201710810
// Arthur Matta	- up
//
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Assinaturas
int main(int argc, char *argv[], char *envp[]);
int getSomething();
void work();


// Main
int main(int argc, char *argv[], char *envp[])
{
	pid_t pid, status;
	if (argc != 2) {
		printf("usage: %s dirname\n",argv[0]);
		exit(1);
	}
	pid=fork();
	if (pid > 0) {
		wait(&status);
		if (WEXITSTATUS(status) == 0) {
			printf("Diretório existente");
		} else if (WEXITSTATUS(status) == -1) {
			printf("Diretório inexistente");
		} else if (WIFSIGNALED(status)) {
			if (WTERMSIG(status) == 6) {
				printf("Terminado por kill.");
			}
		}
		printf("My child did finish the command.\"ls -laR %s\"\n with status code: %d", argv[1], WEXITSTATUS(status));
	}
	else if (pid == 0){
		// execlp("ls", "ls", "-laR", argv[1], NULL);
		int exec = execlp("/bin/ls", "ls", "-laR", argv[1], NULL);
		if (exec == -1) {
			exit(-1);	
		} else if (exec == 0) {
			exit(0);
		}
		
	}
	exit(0);
} 

