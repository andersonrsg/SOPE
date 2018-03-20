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
int main(int argc, char *argv[]);
int getSomething();
void work();
bool saveToFile();


// Main
int main(int argc, char *argv[])
{
	pid_t pid, status;
	bool argI = false;
	bool argL = false;
	bool argN = false;
	bool argC = false;
	bool argW = false;
	bool argR = false;

	if (argc < 3) {
		printf("usage: %s [options] pattern [file/dir]\n",argv[0]);
		exit(1);
	}
	if (argc == 3) {
		// tenta abrir argv[2]
		// se conseguiu, procura por argv[1]
	}
	if (argc > 3) {
		// arquivo = argc[arvc-1]
		for (int i = 1; i < argc-2 ; i++) {
		
			if strcmp(argv[i], "-i") {
				argI = true;
			}
			
			
		}
		// 
		//
		//
		//
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

