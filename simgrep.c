// simgrep.c
//
// Anderson Gralha - up201710810
// Arthur Matta	- up201609953
// Fernando Oliveira -
//
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include <fcntl.h>

#define BIT(n)  (0x01 << n)
#define I_FLAG  BIT(0)
#define L_FLAG  BIT(1)
#define N_FLAG  BIT(2)
#define C_FLAG  BIT(3)
#define W_FLAG  BIT(4)
#define R_FLAG  BIT(5)

typedef struct Analysis Analysis;
struct Analysis {
    
    char *pattern;
    char *matchesfound;
    unsigned long matchesCount;
    
};

// Assinaturas
int main(int argc, char *argv[]);
int getSomething();
int simgrep(char *pattern, char **filenames, unsigned char flags);
int is_file_or_dir(char *curr);
char **getDirContent(char *directory);
// void work();
// bool saveToFile();
char* leArquivoDeEntrada(char *nomeEntrada, unsigned long *tamEntrada);
unsigned long obtemTamanhoDoArquivo(FILE* f);
void leArquivo(FILE* f, char* ptr, unsigned long TamanhoEsperado);
Analysis analyseFile(char* ptr, unsigned long tamanho, char* pattern);

unsigned char flags = 0x00;

// Main
int main(int argc, char *argv[]) {
    
    regex_t regex;
    int re, k=0;
    char msg[100],
    *args = NULL,
    *token,
    *pattern = NULL,
    **files = NULL;
    
    
    /* Concatenate args */
    args = (char*)malloc(strlen(argv[0]));
    strcpy(args, argv[0]);
    for (int i = 1; i < argc; i++) {
        args = (char*)realloc(args, strlen(args)+1+strlen(argv[i])+1);
        strcat(args, " ");
        strcat(args, argv[i]);
    }
    
    /* Generate acceptable input format */
    if (regcomp(&regex, "(\\.\\/simgrep( *-[ilncwr] *)* *[a-zA-Z0-9]+(( +[a-zA-Z0-9]+\\.[a-z]{1,4})|( *\\.{1,2}(\\/[a-zA-Z0-9]+(\\.[a-z]{1,4})?)*\\/*))* *)$", REG_EXTENDED)) {
        fprintf(stderr, "Could not compile regex\n");
        exit(1);
    }
    
    /* test input agains acceptable input format */
    re = regexec(&regex, args, 0, NULL, 0);
    if (!re) {
        
        token = strtok(args, " "); /* Break string into tokens */
        token = strtok(NULL, " "); /* Ignore first token ./simgrep */
        
        while (token != NULL) {	/* Cicle trough remaining tokens */
            if(!strcmp(token, "-i")) flags |= I_FLAG; /* Set I flag */
            else if(!strcmp(token, "-l")) flags |= L_FLAG; /* Set L flag */
            else if(!strcmp(token, "-n")) flags |= N_FLAG; /* Set N flag */
            else if(!strcmp(token, "-c")) flags |= C_FLAG; /* Set C flag */
            else if(!strcmp(token, "-w")) flags |= W_FLAG; /* Set W flag */
            else if(!strcmp(token, "-r")) flags |= R_FLAG; /* Set R flag */
            else {
                re = regcomp(&regex, "^[a-zA-Z0-9]+$", REG_EXTENDED); /* Test if token is a pattern */
                if (re) {
                    fprintf(stderr, "Could not compile regex\n");
                    exit(1);
                }
                re = regexec(&regex, token, 0, NULL, 0);
                
                if(!re) pattern = token;	/* token is a pattern */
                else if(re == REG_NOMATCH){
                    files = (char**)realloc(files, (k+1) * sizeof(char*));
                    files[k] = (char*)malloc(strlen(token)+1);
                    files[k] = token; /* token is a filename */
                    k++;
                }
            }
            
            token = strtok(NULL, " "); /* get next token */
        }
        
        
        if((flags & R_FLAG) && (files == NULL)){
            files = (char**)realloc(files, (k+1) * sizeof(char*));
            *files = ".";
        }
        else if(!(flags & R_FLAG) && (files == NULL)){
            
            /*
             TODO: if R flag is down and files is NULL
             then simgrep should read from stdin
             */
            
            files = (char**)realloc(files, (k+1) * sizeof(char*));
            *files = "stdin";
        }
        else{
            files[k] = NULL; /* set end of files */
        }
        
        
        if(simgrep(pattern, files, flags)){
            perror("simgrep");
            exit(1);
        }
    }
    else if (re == REG_NOMATCH) { /* Input is invalid */
        puts("usage: simgrep [OPTION]... PATTERN [FILE/DIR]");
    }
    else { /* Regex error */
        regerror(re, &regex, msg, sizeof(msg));
        fprintf(stderr, "Regex match failed: %s\n", msg);
        exit(1);
    }
    return 0;
}

int simgrep(char *pattern, char **filenames, unsigned char flags) {
    int i=0, j=0, k=0, r;
    char **directories = NULL,
    **files = NULL,
    **dircontent = NULL;
    pid_t pid;
    
    while(filenames[i] != NULL) {
        /* Check if filename is a file or a directory */
        r = is_file_or_dir(filenames[i]);
        
        if(r == 1) /* filename is a file */
        {
            /* reallocate memory to add new file */
            files = (char**)realloc(files, (j+1) * sizeof(char*));
            /* allocate memory for <filename> + end of string character '\0' */
            files[j] = (char*)malloc(strlen(filenames[i])+1);
            strcpy(files[j], filenames[i]);
            j++;
        }
        else if(r == 0) /* filename is a directory */
        {
            /* reallocate memory to add new directory */
            directories = (char**)realloc(directories, (k+1) * sizeof(char*));
            /* allocate memory for <dirname> + end of string character '\0' */
            directories[k] = (char*)malloc(strlen(filenames[i])+1);
            strcpy(directories[k], filenames[i]);
            k++;
        }
        else /* filename is neither a regular file nor a directory */
        {
            fprintf(stderr, "File %s not found!\n", filenames[i]);
            return 1;
        }
        i++;
    }
    /* add a null terminator to the array */
    files = (char**)realloc(files, (j+1) * sizeof(char*));
    files[j] = NULL;
    
    /* add a null terminator to the array */
    directories = (char**)realloc(directories, (k+1) * sizeof(char*));
    directories[k] = NULL;
    
    j = k = 0;
    /*
     printf("Files: \n");
     while(files[j] != NULL)
     printf("%s\n", files[j++]);
     
     printf("\n");
     
     printf("directories: \n");
     while(directories[k] != NULL)
     printf("%s\n", directories[k++]);
     
     printf("\n");
     */
    
    /* flags options */
    
    if(flags & R_FLAG) {
        // printf("Flag R ativa\n");
        for (i = 0; directories[i] != NULL; i++) {
            if((pid = fork()) < 0){
                fprintf(stderr, "Error in fork\n");
                exit(2);
            }
            else if(pid) {
                waitpid(pid, &r , 0);
            }
            else {
                dircontent = getDirContent(directories[i]);
                
                if((dircontent != NULL) && (dircontent[0] != NULL)){
                    if(simgrep(pattern, dircontent, flags)) {
                        perror("simgrep invocation on child returned an error");
                        exit(1);
                    }
                }
                else
                {
                    printf("getDirContent return null\n");
                }
                free(directories);
                free(files);
                free(dircontent);
                exit(0);
            }
        }
    }

    for (i = 0; files[i] != NULL; i++) {
        unsigned long TamanhoDoArquivo;
        char* Dados;
        
        Dados = leArquivoDeEntrada(files[i], &TamanhoDoArquivo);
        Analysis a = analyseFile(Dados, TamanhoDoArquivo, pattern);
        
        if ((flags & C_FLAG) && !(flags & L_FLAG)) {
            printf("%ld\n", a.matchesCount);
        }
        if ((flags & C_FLAG) && (flags & L_FLAG)) {
            printf("%ld\n", sizeof(files));
        }
        if ((flags & L_FLAG)) {
            printf("%s\n", files[i]);
        }
    }
    
    // printf("\n");
    
    return 0;
}


int is_file_or_dir(char *file){
    struct stat fileStat;
    
    if(stat(file, &fileStat) < 0)
        return -1;
    
    return S_ISREG(fileStat.st_mode);
}

char **getDirContent(char *directory){
    DIR *dir;
    struct dirent *dp;
    int i=0;
    char *filepath = NULL,
    **filenames = NULL;
    
    /* open directory to read its content */
    if((dir = opendir(directory)) == NULL){
        fprintf(stderr, "Cannot open %s\n", directory);
        return NULL;
    }
    
    /* read directory content */
    while((dp = readdir(dir)) != NULL){
        
        /* skip first two entries: "." and ".." as these don't interest us */
        if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")){
            continue;
        }
        
        /* realloc memory to fit "/", "<filename>" and '\0' to the current filepath */
        filepath = (char*)realloc(filepath, strlen(directory)+strlen(dp->d_name)+2);
        
        /* append /<filename> to current filepath */
        filepath[0] = '\0';
        strcpy(filepath, directory);
        strcat(filepath, "/");
        strcat(filepath, dp->d_name);
        
        /* realloc memomy to add new filepath */
        filenames = (char**)realloc(filenames, (i+1)*sizeof(char*));
        
        /* allocate memory to fit filepath + '\0' */
        filenames[i] = (char*)malloc(strlen(filepath)+1);
        
        strcpy(filenames[i], filepath);
        i++;
    }
    /* add a null terminator to the array */
    filenames = (char**)realloc(filenames, (i+1)*sizeof(char*));
    filenames[i] = NULL;
    
    closedir(dir);
    return filenames;
}

char* leArquivoDeEntrada(char *nomeEntrada, unsigned long *tamEntrada) {
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
    char *ptr;
    ptr = (char*)malloc(sizeof(char) * *tamEntrada);
    if(ptr == NULL) { // Testa se conseguiu alocar
        // printf("Erro na alocação da memória!\n");
        exit(1);
    }
    leArquivo(arq, ptr, *tamEntrada);
    fclose(arq); // fecha o arquivo
    return ptr;
}

unsigned long obtemTamanhoDoArquivo(FILE* f) {
    fseek(f, 0, SEEK_END);
    unsigned long len = (unsigned long)ftell(f);
    fseek(f, SEEK_SET, 0);
    return len;
}

void leArquivo(FILE* f, char* ptr, unsigned long TamanhoEsperado) {
    unsigned long NroDeBytesLidos;
    NroDeBytesLidos = fread(ptr, sizeof(char), TamanhoEsperado, f);
    
    if(NroDeBytesLidos != TamanhoEsperado) { // verifica se a leitura funcionou
        // printf("Erro na Leitura do arquivo!\n");
        // printf("Nro de bytes lidos: %ld", NroDeBytesLidos);
        exit(1);
    } else {
        // printf("Leitura realizada com sucesso!\n");
    }
}

Analysis analyseFile(char* ptr, unsigned long tamanho, char* pattern) {
    
    struct Analysis a;
    a.matchesCount = 0;
    unsigned long line = 1;
    
    // printf("pos: %ld, postampatt: %ld, patter: %s\n", pos, posTamPattern, pattern);
    
    a.pattern = (char*)malloc(sizeof(char) * sizeof(pattern));
    a.pattern = pattern;
    
    char *inicioPalavra = ptr;
    
    // Montar String
    
    char *tok;
    char *buffer;
    
    while ( (tok = strsep(&inicioPalavra, "\n")) != NULL) {
        
        char *copy = strdup(tok);
        char *newPattern = strdup(pattern);
        
        int wFlagFound = 0;
        
        if(flags & I_FLAG) {
            char *init = copy;
            for ( ; *init; ++init) *init = tolower(*init);
            
            init = newPattern;
            for ( ; *init; ++init) *init = tolower(*init);
        }
        
        
        char *p = tok;
        size_t n = strlen( newPattern );
        
        if ((p = strstr(copy, newPattern)) != NULL) {
            
            if (flags & W_FLAG) {
                char *q = p + n;
                if ( p == newPattern || isblank (( unsigned char ) *(p - 1))) {
                    if ( *q == '\0' || isblank (( unsigned char ) *q)) {
                        wFlagFound = 1;
                    }
                }
                p = q;
            }
            
            if((flags & N_FLAG) && !(flags & C_FLAG)) {
                const int n = snprintf(NULL, 0, "%lu", line);
                buffer = (char*)malloc(sizeof(char) * n+2);
                snprintf(buffer, n+1, "%lu", line);
                buffer[n] = ':';
                buffer[n+1] = '\0';
            } else {
                buffer = (char*)malloc(sizeof(char));
                buffer = "";
            }
            
            if (flags & W_FLAG) {
                if (wFlagFound) {
                    a.matchesCount++;
                    if (!(flags & C_FLAG)) {
                        printf("%s%s\n", buffer, tok);
                    }
                }
            } else {
                a.matchesCount++;
                if (!(flags & C_FLAG)) {
                    printf("%s%s\n", buffer, tok);
                }
            }
            
            
        }
        
        line++;
        
    }
    
    return a;
}
