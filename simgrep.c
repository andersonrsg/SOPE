// simgrep.c
//
// Anderson Gralha - up201710810
// Arthur Matta	- up
// Fernando Melo - 
//
//
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>


typedef struct Analysis Analysis;
struct Analysis {

	unsigned char *pattern;
	unsigned char *matchesfound;
	unsigned long matchesCount;

};

// Assinaturas
int main(int argc, char *argv[]);
int getSomething();
// void work();
// bool saveToFile();
unsigned char* leArquivoDeEntrada(char *nomeEntrada, unsigned long *tamEntrada);
unsigned long obtemTamanhoDoArquivo(FILE* f);
void leArquivo(FILE* f, unsigned char* ptr, unsigned long TamanhoEsperado);
Analysis analyseFile(unsigned char* ptr, unsigned long tamanho, unsigned char* pattern, unsigned long tamanhoPattern);

// Main
int main(int argc, char *argv[])
{

	// printf("%d\n", CHAR_MIN);

	pid_t pid, status;

	// Ignora Case
	short int argI = 0;

	// Mostrar apenas o nome dos ficheiros onde estiver o padrão a procurar;
	short int argL = 0;

	// indicar também o número das linhas onde estiver o padrão a procurar;
	short int argN = 0;

	// Indicar em quantas linhas o padrão a procurar é encontrado;
	short int argC = 0;

	// O padrão a procurar deve formar uma palavra completa;
	short int argW = 0;

	// Procurar o padrão em todos os ficheiros abaixo da árvore do diretório indicado;
	short int argR = 0;

	unsigned long TamanhoDoArquivo;
    unsigned char* Dados;

	if (argc < 3) {
		printf("usage: %s [options] pattern [file/dir]\n",argv[0]);
		exit(1);
	}

	
    Dados = leArquivoDeEntrada(argv[2], &TamanhoDoArquivo);

    printf("%s\n", Dados);



	if (argc == 3) {
		// tenta abrir argv[2]
		// se conseguiu, procura por argv[1]
	}
	if (argc > 3) {
		// arquivo = argc[arvc-1]
		for (int i = 1; i < argc-2 ; i++) {
		
			if (strcmp(argv[i], "-i") > 0) {
				argI = 1;
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

unsigned char* leArquivoDeEntrada(char *nomeEntrada, unsigned long *tamEntrada)
{
    FILE* arq;
    // Tenta abrir o arquivo
    arq = fopen(nomeEntrada, "rb");
    if(arq == NULL) {
        printf("Arquivo %s não existe.\n", nomeEntrada);
        exit(1);
    }
    *tamEntrada = obtemTamanhoDoArquivo(arq);
    // printf("O tamanho do arquivo %s é %ld bytes.\n", nomeEntrada, *tamEntrada);
    
    // Aloca memória para ler todos os bytes do arquivo
    unsigned char *ptr;
    ptr = (unsigned char*)malloc(sizeof(unsigned char) * *tamEntrada);
    if(ptr == NULL) { // Testa se conseguiu alocar
        // printf("Erro na alocação da memória!\n");
        exit(1);
    }
    leArquivo(arq, ptr, *tamEntrada);
    fclose(arq); // fecha o arquivo
    return ptr;
}

unsigned long obtemTamanhoDoArquivo(FILE* f)
{
    fseek(f, 0, SEEK_END);
    unsigned long len = (unsigned long)ftell(f);
    fseek(f, SEEK_SET, 0);
    return len;
}

void leArquivo(FILE* f, unsigned char* ptr, unsigned long TamanhoEsperado)
{
    unsigned long NroDeBytesLidos;
    NroDeBytesLidos = fread(ptr, sizeof(unsigned char), TamanhoEsperado, f);
    
    if(NroDeBytesLidos != TamanhoEsperado) { // verifica se a leitura funcionou
         printf("Erro na Leitura do arquivo!\n");
         printf("Nro de bytes lidos: %ld", NroDeBytesLidos);
        exit(1);
    } else {
         printf("Leitura realizada com sucesso!\n");
    }
}

Analysis analyseFile(unsigned char* ptr, unsigned long tamanho, unsigned char* pattern, unsigned long tamanhoPattern) {

	struct Analysis a;
	unsigned long pos = 0;
	unsigned long posTamPattern = 0;

	unsigned char *inicioPalavra = ptr;
	unsigned char *inicioPalavraPattern = pattern;

	// unsigned char *pos = ptr;

	while (pos < tamanho) {
		if (inicioPalavra == inicioPalavraPattern) {

			inicioPalavra++;
			inicioPalavraPattern++;
			posTamPattern++;

		} else {
			inicioPalavra = &ptr[pos];
			inicioPalavraPattern = pattern;
			posTamPattern = 0;
		}

		if (tamanhoPattern == posTamPattern) {
			a.matchesCount++;
		}

		pos++;
	}


	return a;
}

















