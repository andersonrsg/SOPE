// simgrep.c
//
// Anderson Gralha - up201710810
// Arthur Matta	- up201609953
// Fernando Oliveira - up201005231
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
#include <signal.h>

#define MAXLEN  256
#define BIT(n)  (0x01 << n)
#define I_FLAG  BIT(0)
#define L_FLAG  BIT(1)
#define N_FLAG  BIT(2)
#define C_FLAG  BIT(3)
#define W_FLAG  BIT(4)
#define R_FLAG  BIT(5)

void sigint_handler(int signo)
{
    printf("\nAre you sure you want to terminate the program? (Y/N)");
    char input;

    do {
        input = getchar();
        input = tolower(input);
    } while(input != 121 && input != 110);

    if (input == 121) {
        // Sends a signal to the group of the calling process.
        // All child process from a parent will have the same group id.
        killpg(getpgrp() ,SIGKILL);
        exit(0);
    } else {
        killpg(getpgrp() ,SIGCONT);
    }
}

void sigintChildHandler(int signo)
{
    if (raise(SIGTSTP) != 0) {
        perror("Failed to pause all processes.");
        exit(0);
    }
}

// Assinaturas
int main(int argc, char *argv[]);
int getSomething();
int simgrep(char *pattern, char **filenames, unsigned char flags);
int is_file_or_dir(char *curr);
char **getDirContent(char *directory);
int grep(char *pattern, char *file, unsigned char flags);

unsigned char flags = 0x00;

/*
    TODO:
    signal handlers:
        fazer processos filhos esperarem?
        task finish
    log file
*/

// Main
int main(int argc, char *argv[]) {
    regex_t regex;
    int re, k=0;
    char msg[100],
    *args = NULL,
    *token,
    *pattern = NULL,
    **files = NULL;

    struct sigaction action;
    action.sa_handler = sigint_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

    if (signal(SIGINT,sigint_handler) < 0)
    {
        fprintf(stderr,"Unable to install SIGINT handler\n");
        exit(1);
    }


    /* Concatenate args */
    args = (char*)malloc(strlen(argv[0]));
    strcpy(args, argv[0]);
    for (int i = 1; i < argc; i++) {
        args = (char*)realloc(args, strlen(args)+1+strlen(argv[i])+1);
        strcat(args, " ");
        strcat(args, argv[i]);
    }

    /* Generate acceptable input format */
    if (regcomp(&regex, "(\\.\\/simgrep( *-[ilncwr] *)* *[a-zA-Z0-9]+( +(\\.{1,2}\\/*)?([a-zA-Z0-9]+(\\.[a-z]{1,4}|\\/*))*)*)$", REG_EXTENDED)) {
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
                /* Test if token is a pattern */
                if (regcomp(&regex, "^[a-zA-Z0-9]+$", REG_EXTENDED)) {
                    fprintf(stderr, "Could not compile regex\n");
                    exit(1);
                }

                if(!regexec(&regex, token, 0, NULL, 0) && (pattern == NULL)) pattern = token;	/* token is a pattern */
                else{ /* token is a filename */
                    files = (char**)realloc(files, (k+1) * sizeof(char*));
                    files[k] = (char*)malloc(strlen(token)+1);
                    files[k] = token;
                    k++;
                }
            }

            token = strtok(NULL, " "); /* get next token */
        }


        if((flags & R_FLAG) && (files == NULL)){
            files = (char**)realloc(files, sizeof(char*));
            files[k++] = ".";
        }
        else if(!(flags & R_FLAG) && (files == NULL)){
            files = (char**)realloc(files, sizeof(char*));
            files[k++] = "stdin";
        }
        files[k] = NULL; /* set end of files */

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
    int i=0, r, matches;
    char **directories = NULL,
         **files = NULL,
         **dircontent = NULL,
         *buffer = NULL;
    pid_t pid;

    if(!strcmp(*filenames, "stdin")){
        return grep(pattern, "stdin", flags);
    }

    while(filenames[i] != NULL) {

        /* Check if filename is a file or a directory */
        r = is_file_or_dir(filenames[i]);

        if(r == 1) { /* filename is a file */
            if ((matches = grep(pattern, filenames[i], flags)) < 0) {
                perror("simgrep: grep");
                exit(1);
            }

            if((flags & C_FLAG) && (flags & L_FLAG)) {
                buffer = (char*)realloc(buffer, sizeof(char) * 3);
                buffer[0] = ':';
                if (matches > 0) {
                    buffer[1] = '1';
                } else {
                    buffer[1] = '0';
                }

                buffer[2] = '\0';

                printf("%s:%s\n", filenames[i], buffer);
                printf("%s\n", filenames[i]);
            }
            else if ((flags & L_FLAG) && (matches > 0)) {
                printf("%s\n", filenames[i]);
            }
            else if ((flags & C_FLAG)) {
                printf("%s:%d\n", filenames[i], matches);
            }
        }
        else if(r == 0){ /* filename is a directory */
            if((pid = fork()) < 0){
                fprintf(stderr, "Error in fork\n");
                exit(2);
            }
            else if(pid) {
                waitpid(pid, &r , 0);
                sleep(10);
            }
            else {
                if (signal(SIGINT,sigintChildHandler) < 0)
                {
                    fprintf(stderr,"Unable to install SIGINT handler\n");
                    exit(1);
                }

                dircontent = getDirContent(filenames[i]);

                if((dircontent != NULL) && (dircontent[0] != NULL)){
                    if(simgrep(pattern, dircontent, flags)) {
                        perror("simgrep invocation on child returned an error");
                        exit(1);
                    }
                }

                // sleep(10);

                free(directories);
                free(files);
                free(dircontent);
                exit(0);
            }
        }
        else{ /* filename is neither a regular file nor a directory */
            fprintf(stderr, "File %s not found!\n", filenames[i]);
            //return 1;
        }
        i++;
    }
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

int grep(char *pattern, char* file, unsigned char flags){
    FILE *ifp; /* Input File pointer */
    char *input = NULL, /* input */
         *match = NULL, /* matched string */
         *line = NULL, /* line string */
         *cpy_input = NULL, /* input copy */
         *cpy_pattern = NULL; /* pattern copy */
    int l = 1, /*  line counter */
        n = 0, /* line string size */
        wflag = 0, /* wflag control */
        matches = 0; /* number of matches */

    /* open I/O file pointers */
    if (strcmp(file, "stdin")) {
        if ((ifp = fopen(file, "r")) == NULL) {
            printf("file %s was not found\n", file);
            return -1;
        }
    }
    else{
        ifp = stdin;
    }

    /* copy pattern to cpy_pattern to make changes without changing the original string */
    cpy_pattern = strdup(pattern);

    input = (char*)malloc(MAXLEN * sizeof(char) + 1);
    /* read file line by line */
    while(fgets(input, MAXLEN, ifp)){

        if((ifp == stdin) && (flags & L_FLAG)){
            printf("(standard input)\n");
            return 0;
        }

        if (input[strlen(input)-1] == '\n') {
            input[strlen(input)-1] = '\0';
        }

        /* copy input to cpy_input to make changes without changing the original string */
        cpy_input = strdup(input);

        /* if i flag is set, convert input and pattern to lowercases */
        if(flags & I_FLAG) {
            char *lower = cpy_input;
            for ( ; *lower; ++lower) *lower = tolower(*lower);

            lower = cpy_pattern;
            for ( ; *lower; ++lower) *lower = tolower(*lower);
        }

        /* check if pattern is a substring of input */
        if((match = strstr(cpy_input, cpy_pattern)) != NULL){

            /* if flag n is set, set line to store line number */
            if((flags & N_FLAG) && !(flags & C_FLAG)){
                n = snprintf(NULL, 0, "%d", l);
                line = (char*)realloc(line, sizeof(char) * n + 2);
                snprintf(line, n+1, "%d", l);
                line[n] = ':';
                line[n+1] = '\0';
            }
            else{
                if (line == NULL) line = (char*)realloc(line, sizeof(char));
                line = "";
            }

            /* if w flag is set, check if match is from a whole word */
            if (flags & W_FLAG) {
                char *q = match + strlen(cpy_pattern);
                if ( !strcmp(match, cpy_pattern) || !isalnum (( unsigned char ) *(match - 1))) {
                    if ( *q == '\0' || !isalnum (( unsigned char ) *q)) {
                        wflag = 1;
                    }
                }
            }

            /* if w flag is set, only print if whole word was found (controlled by wflag variable) */
            if (flags & W_FLAG) {
                if (wflag) {
                    matches++;
                    if (!(flags & C_FLAG) && !(flags & L_FLAG)) {
                        printf("%s%s\n", line, input);
                    }
                    wflag = 0;
                }
            }
            else if(flags & R_FLAG){
                if (!(flags & C_FLAG) && !(flags & L_FLAG)) {
                    printf("%s:%s%s\n", file, line, input);
                }
            }
            else {
                matches++;
                if (!(flags & C_FLAG) && !(flags & L_FLAG)) {
                    printf("%s%s\n", line, input);
                }
            }

            /* increment line counter */
            l++;
        }
    }

    /* close File pointers */
    fclose(ifp);

    /* return number of matched strings */
    return matches;
}
