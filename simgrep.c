// simgrep.c
//
// Anderson Gralha - up201710810
// Arthur Matta	- up201609953
//
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <regex.h>


typedef struct Analysis Analysis;
struct Analysis {

	unsigned char *pattern;
	unsigned char *matchesfound;
	unsigned long matchesCount;

};

// Assinaturas
int main(int argc, char *argv[]);
int getSomething();
void simgrep(char *pattern, char **files, short int *flags);
// void work();
// bool saveToFile();
unsigned char* leArquivoDeEntrada(char *nomeEntrada, unsigned long *tamEntrada);
unsigned long obtemTamanhoDoArquivo(FILE* f);
void leArquivo(FILE* f, unsigned char* ptr, unsigned long TamanhoEsperado);
Analysis analyseFile(unsigned char* ptr, unsigned long tamanho, unsigned char* pattern, unsigned long tamanhoPattern);

// Main
int main(int argc, char *argv[])
{
	pid_t pid, status;
	short int flags[6] = {0,0,0,0,0,0};
	regex_t regex;
	int re, k=0;
	char msg[100];
	char string[100];
	char *token;
	char *pattern = NULL;
	char *files[50];

	/* Concatenate args */
	strcpy(string, argv[0]);
	for (int i = 1; i < argc; i++) {
		strcat(string, " ");
		strcat(string, argv[i]);
	}

	/* Generate acceptable input format */
	re = regcomp(&regex, "(\\.\\/simgrep(\\s*-[ilncwr] *)*\\s*(\\w)+( +(\\w)*\\.[a-z]{1,4})* *)$", REG_EXTENDED);
	if (re) {
		fprintf(stderr, "Could not compile regex\n");
		exit(1);
	}

	/* test input agains acceptable input format */
	re = regexec(&regex, string, 0, NULL, 0);
	if (!re) {

		token = strtok(string, " "); /* Break string into tokens */
		token = strtok(NULL, " "); /* Ignore first token ./simgrep */

		while (token != NULL) {	/* Cicle trough remaining tokens */
			if(!strcmp(token, "-i")) flags[0] = 1; /* Set I flag */
			else if(!strcmp(token, "-l")) flags[1] = 1; /* Set L flag */
			else if(!strcmp(token, "-n")) flags[2] = 1; /* Set N flag */
			else if(!strcmp(token, "-c")) flags[3] = 1; /* Set C flag */
			else if(!strcmp(token, "-w")) flags[4] = 1; /* Set W flag */
			else if(!strcmp(token, "-r")) flags[5] = 1; /* Set R flag */
			else {
				re = regcomp(&regex, "^\\w+$", REG_EXTENDED); /* Test if token is a pattern */
				if (re) {
					fprintf(stderr, "Could not compile regex\n");
					exit(1);
				}
				re = regexec(&regex, token, 0, NULL, 0);

				if(!re) pattern = token;	/* token is a pattern */
				else if(re == REG_NOMATCH) files[k++] = token; /* token is a filename */
			}

			files[k] = NULL;

			token = strtok(NULL, " "); /* get next token */
		}

		simgrep(pattern, files, flags);
	}
	else if (re == REG_NOMATCH) { /* Input is invalid */
		puts("usage: simgrep [OPTION]... PATTERN [FILE/DIR]");
	}
	else { /* Regex error */
		regerror(re, &regex, msg, sizeof(msg));
		fprintf(stderr, "Regex match failed: %s\n", msg);
		exit(1);
	}


}


void simgrep(char *pattern, char **files, short int *flags){
	int i = 0;

	printf("\nSIMGREP ARGS\n\n");

	printf("Pattern: %s\n\n", pattern);

	while(files[i] != NULL)
	{
		printf("file %d: %s\n", i, files[i]);
		i++;
	}
	printf("\n");

	if(flags[0]) printf("Flag I ativa\n");
	if(flags[1]) printf("Flag L ativa\n");
	if(flags[2]) printf("Flag N ativa\n");
	if(flags[3]) printf("Flag C ativa\n");
	if(flags[4]) printf("Flag W ativa\n");
	if(flags[5]) printf("Flag R ativa\n");

	printf("\n");

	exit(0);
}

/*
}
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
*/
