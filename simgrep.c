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
#include <errno.h>
#include <time.h>

#define MAXLEN  256
#define BIT(n)  (0x01 << n)
#define I_FLAG  BIT(0)
#define L_FLAG  BIT(1)
#define N_FLAG  BIT(2)
#define C_FLAG  BIT(3)
#define W_FLAG  BIT(4)
#define R_FLAG  BIT(5)


// Assinaturas
int main(int argc, char *argv[]);
int simgrep(char *pattern, char **filenames, unsigned char flags);
int is_file_or_dir(char *curr);
char **getDirContent(char *directory);
int grep(char *pattern, char *file, unsigned char flags);
void sigint_handler(int signo);
void sigintChildHandler(int signo);
int get_number_size(size_t number);
void write_to_logfile(int pid, char* message, char* arg);

unsigned char flags = 0x00;
clock_t begin;

// Main
int main(int argc, char *argv[]) {
    regex_t re_simgrep, re_options, re_pattern, re_files;
    int k=0;
    char *pattern = NULL,
    **files = NULL,
    *args = NULL;

    begin = clock();

    args = (char*)malloc(strlen(argv[0]) + 1);
    strcpy(args, argv[0]);
    for (int i = 1; i < argc; i++) {
        args = (char*)realloc(args, strlen(args) + strlen(argv[i]) + 1);
        strcat(args, " ");
        strcat(args, argv[i]);
    }

    write_to_logfile(getpid(), "COMANDO ", args);

    struct sigaction action;
    action.sa_handler = sigint_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;


    if (signal(SIGINT,sigint_handler) < 0)
    {
        perror("sigint_handler");
        exit(1);
    }
    if (regcomp(&re_simgrep, "\\.\\/simgrep", REG_EXTENDED)){
        perror("simgrep: re_simgrep");
        exit(1);
    }
    else if(regcomp(&re_options, "-[ilncwr]+", REG_EXTENDED)){
        perror("simgrep: re_options");
        exit(1);
    }
    else if(regcomp(&re_pattern, "[a-zA-Z0-9]", REG_EXTENDED)){
        perror("simgrep: re_pattern");
        exit(1);
    }
    else if(regcomp(&re_files, "((\\.{1,2}\\/*)?([a-zA-Z0-9]+(\\.[a-z]{1,4}|\\/*))*)*", REG_EXTENDED)){
        perror("simgrep: re_files");
        exit(1);
    }


    for (int i = 0; i < argc; i++) {
        if (!regexec(&re_simgrep, argv[i], 0, NULL, 0)) {
            continue;
        }
        else if(!regexec(&re_options, argv[i], 0, NULL, 0)){
            for (int j = 1; j < strlen(argv[i]); j++) {
                if(argv[i][j] == 'i') flags |= I_FLAG;
                else if(argv[i][j] == 'l') flags |= L_FLAG;
                else if(argv[i][j] == 'n') flags |= N_FLAG;
                else if(argv[i][j] == 'c') flags |= C_FLAG;
                else if(argv[i][j] == 'w') flags |= W_FLAG;
                else if(argv[i][j] == 'r') flags |= R_FLAG;
                else{
                    puts("usage: simgrep [OPTION]... PATTERN [FILE/DIR]");
                    exit(1);
                }
            }
        }
        else if(!regexec(&re_pattern, argv[i], 0, NULL, 0) && (pattern == NULL)){
            pattern = argv[i];
        }
        else if(!regexec(&re_files, argv[i], 0, NULL, 0)){
            files = (char**)realloc(files, (k+1) * sizeof(char*));
            files[k] = (char*)malloc(strlen(argv[i])+1);
            files[k] = argv[i];
            k++;
        }
        else{
            puts("usage: simgrep [OPTION]... PATTERN [FILE/DIR]");
            exit(1);
        }
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

    return 0;
}

int simgrep(char *pattern, char **filenames, unsigned char flags) {
    int i=0, f=0, d=0, r, matches;
    char **files = NULL,
    **directories = NULL,
    **dircontent = NULL;
    pid_t pid;

    if(!strcmp(*filenames, "stdin")){
        return grep(pattern, "stdin", flags);
    }

    while(filenames[i] != NULL) {
        /* Check if filename is a file or a directory */
        r = is_file_or_dir(filenames[i]);

        if(r == 1) { /* filename is a file */
            files = (char**)realloc(files, (f+1) * sizeof(char*));
            files[f] = (char*)malloc(strlen(filenames[i])+1);
            strcpy(files[f], filenames[i]);
            f++;
        }
        else if(r == 0){ /* filename is a directory */
            directories = (char**)realloc(directories, (d+1) * sizeof(char*));
            directories[d] = (char*)malloc(strlen(filenames[i])+1);
            strcpy(directories[d], filenames[i]);
            d++;
        }
        else{ /* filename is neither a regular file nor a directory */
            fprintf(stderr, "simgrep: %s: %s\n", filenames[i], strerror(errno));
            //return 1;
        }
        i++;
    }

    files = (char**)realloc(files, (f+1) * sizeof(char*));
    files[f] = NULL;

    directories = (char**)realloc(directories, (d+1) * sizeof(char*));
    directories[d] = NULL;

    for (i = 0; directories[i] != NULL; i++) {
        if (flags & R_FLAG) {
            if((pid = fork()) < 0) {
                perror("simgrep: fork");
                exit(2);
            }
            else if(pid == 0){
                if (signal(SIGINT,sigintChildHandler) < 0)
                {
                    perror("simgrep: sigintChildHandler");
                    exit(1);
                }

                dircontent = getDirContent(directories[i]);

                if((dircontent != NULL) && (dircontent[0] != NULL)){
                    if(simgrep(pattern, dircontent, flags)) {
                        perror("simgrep");
                        exit(1);
                    }
                }
                free(dircontent);
                exit(0);
            }
            else{
                sleep(5);
            }
        }
        else{
            fprintf(stderr, "simgrep: %s: Is a directory\n", directories[i]);
        }
    }


    for(i=0; files[i] != NULL; i++){
        if ((matches = grep(pattern, files[i], flags)) < 0) {
            perror("simgrep: grep");
            exit(1);
        }

        if (matches > 0) {
            if (flags & L_FLAG) {
                printf("%s\n", files[i]);
            }
            else if (flags & C_FLAG) {
                printf("%s:%d\n", files[i], matches);
            }
        }
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
    if((dir = opendir(directory)) == NULL) {
        // getTime();
        // write(fd, "FALHA AO ABRIR DIRETORIO ", 24);
        write_to_logfile(getpid(), "FALHA AO ABRIR DIRETORIO ", directory);
        fprintf(stderr, "getDirContent: %s: %s\n", directory, strerror(errno));
        return NULL;
    }
    write_to_logfile(getpid(), "ABERTO DIRETORIO ", directory);

    /* read directory content */
    while((dp = readdir(dir)) != NULL){

        /* skip first two entries: "." and ".." as these don't interest us */
        if(!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..")){
            continue;
        }

        /* realloc memory to fit <filename>" and '\0' to the current filepath */
        filepath = (char*)realloc(filepath, strlen(directory)+strlen(dp->d_name)+ ((directory[strlen(directory)-1] == '/')? 1 : 2 ));

        /* append /<filename> to current filepath */
        filepath[0] = '\0';
        strcpy(filepath, directory);
        if(directory[strlen(directory)-1] != '/') strcat(filepath, "/");
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
    write_to_logfile(getpid(), "FECHADO DIRETORIO ", directory);
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
    matches = 0; /* number of matches */

    /* open I/O file pointers */
    if (strcmp(file, "stdin")) {
        if ((ifp = fopen(file, "r")) == NULL) {
            write_to_logfile(getpid(), "FALHA AO ABRIR ARQUIVO ", file);
            return -1;
        } else {
            write_to_logfile(getpid(), "ABERTO ARQUIVO ", file);
        }
    }
    else {
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
            matches++;
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

            if (!(flags & (C_FLAG | L_FLAG))) {
                if (flags & W_FLAG) {
                    char *q = match + strlen(cpy_pattern);
                    if ( !strcmp(match, cpy_pattern) || !isalnum (( unsigned char ) *(match - 1))) {
                        if ( *q == '\0' || !isalnum (( unsigned char ) *q)) {
                            if (flags & R_FLAG)
                            printf("%s:%s%s\n", file, line, input);
                            else
                            printf("%s%s\n", line, input);
                        }
                    }
                }
                else if(flags & R_FLAG){
                    printf("%s:%s%s\n", file, line, input);
                }
                else {
                    printf("%s%s\n", line, input);
                }
            }

            /* increment line counter */
            l++;
        }

    }

    /* close File pointers */
    fclose(ifp);
    write_to_logfile(getpid(), "FECHADO ARQUIVO ", file);


    /* return number of matched strings */
    return matches;
}

void sigint_handler(int signo){
    printf("\nAre you sure you want to terminate the program? (Y/N)");
    char input, *message;

    do {
        input = getchar();
        input = tolower(input);
    } while(input != 121 && input != 110);

    if (input == 121) {
        // Sends a signal to the group of the calling process.
        // All child process from a parent will have the same group id.
        message = (char*)malloc(strlen("SIGKILL to ") + get_number_size(getpgrp()) + 1);
        sprintf(message, "SIGKILL to %d", getpgrp());
        write_to_logfile(getpid(), "SIGNAL ", message);
        killpg(getpgrp() ,SIGKILL);
        exit(0);
    } else {
        message = (char*)malloc(strlen("SIGCONT to ") + get_number_size(getpgrp()) + 1);
        sprintf(message, "SIGCONT to %d", getpgrp());
        write_to_logfile(getpid(), "SIGNAL ", message);
        killpg(getpgrp() ,SIGCONT);
    }
}

void sigintChildHandler(int signo){
    if (raise(SIGTSTP) != 0) {
        perror("Failed to pause all processes.");
        exit(0);
    }
}

int get_number_size(size_t number){
    int i = 0;
    do {
        number /= 10;
        i++;
    } while(number > 0);
    return i;
}

void write_to_logfile(int pid, char* message, char* arg){
    int fd, msg_size;
    time_t rawtime;
    struct tm *timeinfo;
    char *to_print = NULL,
    currentTime[20],
    str_pid[ get_number_size(getpid()) ];
    clock_t end;
    double timespent;

    /*
        TODO:
        change time to miliseconds since beginning of execution (?)
    */

    fd = open("logfile.txt", O_WRONLY | O_APPEND | O_CREAT, 0644);

    // end = clock();
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(currentTime, 20, "%x_%X", timeinfo);

    // timespent = (double)(end - begin)*1000/CLOCKS_PER_SEC;

    sprintf(str_pid, "%d", pid);

    msg_size = strlen(currentTime) + strlen(" - ") + strlen("PID ") + strlen(str_pid) + strlen(" - ") + strlen(message) + strlen(arg) + 2;
    // msg_size = get_number_size(timespent) + strlen(" - ") + strlen("PID ") + strlen(str_pid) + strlen(" - ") + strlen(message) + strlen(arg) + 2;

    to_print = (char*)malloc(msg_size);
    // sprintf(to_print, "%f", timespent);
    strcpy(to_print, currentTime);
    strcat(to_print, " - ");
    strcat(to_print, "PID ");
    strcat(to_print, str_pid);
    strcat(to_print, " - ");
    strcat(to_print, message);
    strcat(to_print, arg);
    strcat(to_print, "\n");

    write(fd, to_print, strlen(to_print));

    close(fd);
}
