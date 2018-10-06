// mysh.c ... a small shell
// Started by John Shepherd, September 2018
// Completed by Ravija Maheshwari, September/October 2018

//CAT on multiple files

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <glob.h>
#include <assert.h>
#include <fcntl.h>
#include "history.h"
#include <wordexp.h>

#define DEBUG 1

#ifdef DEBUG
#  define D(x) x
#else
#  define D(x)
#endif

// This is defined in string.h
// BUT ONLY if you use -std=gnu99
//extern char *strdup(char *);

// Function forward references

void trim(char *);
int strContains(char *, char *);
char **tokenise(char *, char *);
char **fileNameExpand(char **);
void freeTokens(char **);
char *findExecutable(char *, char **);
int isExecutable(char *);
void prompt(void);
void execute(char **, char **, char **, char*, int[]);
void hyphenate();

// Global Constants

#define MAXLINE 200

// Global Data

static char *const HOME_DIR = "/Users/manishm";
int nextSequence = 0; 

int main(int argc, char *argv[], char *envp[])
{
    pid_t pid;          // pid of child process
    int stat;           // return status of child
    char **path;        // array of directory names
    int nextSequence;   // command number
    int i;              // generic index
    char cwd[1024];
    

    // set up command PATH from environment variable
    for (i = 0; envp[i] != NULL; i++) {
        if (strncmp(envp[i], "PATH=", 5) == 0) break;
    }
    if (envp[i] == NULL){
        path = tokenise("/bin:/usr/bin",":");
    }else{
        path = tokenise(&envp[i][5],":");
    }

    // initialise command history
    nextSequence = initCommandHistory() + 1;

    char line[MAXLINE];
    prompt();

    while (fgets(line, MAXLINE, stdin) != NULL) {
        startagain: trim(line);

        if (strcmp(line,"exit") == 0) {
            break;
        }

        if (strcmp(line,"") == 0) {
            prompt();
            continue;
        }

        //Tokenise the command line treating 'whitespace' as delimiter
        char** tokenised_line = tokenise(line, " ");

        //###############//HISTORY SUBSTITUTIONS//###############//
        
        //executing the last command from history
        if(strcmp(tokenised_line[0],"!!") == 0){
            //Execute last command from history.
            int lastSeq = getSeqOfLastCommandFromHistory();
            if(lastSeq == -1){
                D(printf("Command not found in history\n"));
            }else{
                char* commandFromHistory = getCommandFromHistory(lastSeq);
                strcpy(line, commandFromHistory);
                D(printf("Executing from history : %s\n", line));
                goto startagain;
            }
        //executing !SeqNo command
        }else if(tokenised_line[0][0] == '!' && isdigit(tokenised_line[0][1])) {
            D(printf("in !SEQ: \n"));
            int seqFromHistory = atoi(tokenised_line[0]+1);
            D(printf("Seq from history = %d\n", seqFromHistory));            
            char *commandFromHistory = getCommandFromHistory(seqFromHistory);
            if(commandFromHistory == NULL){
                printf("No command #%d\n", seqFromHistory);
            } else{
                strcpy(line, commandFromHistory);
                D(printf("Executing seq number %d from history : %s\n", seqFromHistory, line));
                goto startagain;
            }
        }else if(tokenised_line[0][0] == '!' && !isdigit(tokenised_line[0][1])){
            printf("Invalid history substitution\n");
        }


        //###############// WILDCARD EXPANSION //###############//
        //Expand the tokenised line for wildcards
        char** expanded_line = fileNameExpand(tokenised_line);

        //################// SHELL BUILT-INS //#################//

        //Implementing pwd
        if(strcmp(expanded_line[0],"pwd") == 0){
            char entire_command[MAXLINE];
            for(int i = 0 ; expanded_line[i] != NULL ; i++){
                strcat(entire_command,expanded_line[i]);
                strcat(entire_command," ");
            }
            D(printf("Entire command is %s\n", entire_command));
            addToCommandHistory(entire_command, nextSequence++);
            D(printf("NEXT SEQ is : %d\n" , nextSequence));
            getcwd( cwd, sizeof(cwd));
            printf("%s\n", cwd);
        //Implementing cd
        }else if(strcmp(expanded_line[0],"cd") == 0) { 
            if (expanded_line[1] == NULL) {
                setenv("HOME", HOME_DIR, 1);
                char *home = getenv("HOME");
                D(printf("Changed to home directory : %s\n", home));
                chdir(home);
                addToCommandHistory(expanded_line[0], nextSequence++);
            }else if(expanded_line[1] != NULL ){
                char *dir_name = expanded_line[1];
                if(chdir(dir_name) == 0){
                    getcwd(cwd, sizeof(cwd));
                    printf("%s\n", cwd);
                    char entire_command[MAXLINE];
                    sprintf(entire_command,"cd %s", dir_name);
                    addToCommandHistory(entire_command, nextSequence++);
                }else{
                    printf("No such file or directory\n");
                }     
            }
        }else if(strcmp(expanded_line[0],"h") == 0 || strcmp(expanded_line[0],"history") == 0){   
            showCommandHistory();
            addToCommandHistory(expanded_line[0], nextSequence++);
        }else{
        //###############// I/O REDIRECTION and COMMAND EXECUTION //###############//

            //File descriptor array. (Index 0 -> write , Index 1 -> read)
            int fd[2];
            // int foundRedirection = 0;
            // int length = 0;
            //Pipe error
            if (pipe(fd) == -1){
                fprintf(stderr, "Pipe Failed" );
                return 1;
            }

            // for(int i = 0; expanded_line[i] != NULL; i++){
            //     if( (strcmp(expanded_line[i], ">") == 0) || (strcmp(expanded_line[i], "<") == 0)){
            //         foundRedirection = 1;
            //         length++;
            //     }
            // }
            //D(printf("length = %d\n", length));
            int code = pid = fork();
            if (code != 0) {
                //Parent process
                printf("in parent hdsfg\n");
                wait(&stat);
                if (WIFEXITED(stat)) {
                    printf("lhegv\n");
                    hyphenate();
                    printf("Returns %d\n", WEXITSTATUS(stat));
                    close(fd[1]);  //closing write descriptor for parent
                    char recd_message[MAXLINE];
                    read(fd[0], recd_message, MAXLINE);
                    close(fd[0]);  //close the read-descriptor for parent

                    if (WEXITSTATUS(stat) == 0) {
                        D(printf("PARENT received message: %s\n", recd_message));
                        addToCommandHistory(recd_message, nextSequence++);
                    }
                } else {
                    hyphenate();
                    printf("%s: Command not found\n",expanded_line[0]);
                }
                freeTokens(tokenised_line);       
            } else {
                //Child process
                printf("In child \n");
                execute(expanded_line, path, envp, line, fd);
            }
        }
        prompt();
    }
    saveCommandHistory();
    cleanCommandHistory();
    return(EXIT_SUCCESS);
}



void execute(char **args, char **path, char **envp, char* untokenised_line, int fd[]){

    int last_token_index = 0;
    int args_length = 0;
    // int foundRedirection = 0;

    while( args[args_length] != NULL ){
        last_token_index = args_length;
        args_length++;
        // if((strcmp(args[args_length], ">") == 0) || (strcmp(args[args_length], "<") == 0)) {
        //     foundRedirection = 1;
        // }
    }
    
    D(printf("Second last is %s Token and last token is %s\n", args[last_token_index-1], args[last_token_index]));
    D(printf("Args length %d\n", args_length));

    char* cmd;
    int execFound = 0;

    // if((foundRedirection == 1) && (args_length == 1)){
    //     printf("Invalid i/o redirection\n");
    //     EXIT_SUCCESS;
    // }

    //Checking if args** conatins an executable command
    if(args[0][0] == '/' || args[0][0] == '.'){
        if(isExecutable(args[0])){
            cmd = args[0];
            execFound = 1;
        }
    }else{
        for(int i = 0 ; path[i] != NULL; i++){
            char* D = path[i];
            sprintf(D, "%s/%s", path[i], args[0]);
            if( (strcmp(D,path[i]) == 0) && isExecutable(D) ){
                cmd = D;
                execFound = 1;
            }
            if(execFound == 1){
                break;
            }
        }
    }


    if (execFound == 0){
        hyphenate();
        printf("%s: Command not found\n", args[0]);
        exit (EXIT_FAILURE);
    }else{
        printf("Running %s ...\n", cmd);
        hyphenate();
        close(fd[0]); //Close read descriptor for child
        write(fd[1], untokenised_line, strlen(untokenised_line)+1);
        D(printf("CHILD (%d) send message: %s\n", getpid(), untokenised_line));
        close(fd[1]); // close the write descriptor for child


        int newfd;
        //Output Re-direction 
        if(args_length > 2 && strcmp(args[last_token_index - 1], ">") == 0){
            
            //Removing '>' and fileName tokens
            char* updated_args[args_length-1];
            for(int j = 0 ; j < (args_length-2) ; j++){
                updated_args[j] = args[j];
            }
            updated_args[args_length-2] = NULL;

            for(int j = 0 ; updated_args[j] != NULL ; j++){
                D(printf("Output redirection -> Updated Args[%d] = %s\n", j, updated_args[j]));
            }

            if ((newfd = open(args[last_token_index], O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {
                perror(args[last_token_index]);
                exit(1);
            }

            int saved = dup(STDOUT_FILENO);
            dup2(newfd, STDOUT_FILENO);

            if(execve(cmd, updated_args, envp) == -1){
                perror("Exec failed\n");
            }

            dup2(saved, STDOUT_FILENO); //restoring

            
        // } else if((args_length <= 2) || (foundRedirection)){
        //     printf("Invalid i/o redirection\n");
        } else if(args_length > 2 && strcmp(args[last_token_index - 1], "<") == 0){
            //Input redirection 
            char* updated_args[args_length-1];

            for(int j = 0 ; j < (args_length-2) ; j++){
                updated_args[j] = args[j];
            }

            updated_args[args_length-2] = NULL;
            for(int j = 0 ; updated_args[j] != NULL ; j++){
                D(printf("Input redirection -> Updated Args[%d] = %s\n", j, updated_args[j]));
            }

            if ((newfd = open(args[last_token_index], O_RDONLY)) < 0) {
                perror(args[last_token_index]);
                exit(1);
            }

            int saved = dup(STDIN_FILENO); //Saving original
            close(STDIN_FILENO);

            dup2(newfd, STDIN_FILENO);

            if(execve(cmd, updated_args, envp) == -1){
                perror("Exec failed\n");
            }

            dup2(saved, STDIN_FILENO); //Restoring
            close(newfd);
        // } else if(args_length <= 2) {
        //     printf("Invalid i/o redirection\n");
        }else{
            //No redirection
            if(execve(cmd, args, envp) == -1){
                perror("Exec failed\n");
            }
        }
        exit (EXIT_SUCCESS);    
    }
}

// isExecutable: check whether this process can execute a file
int isExecutable(char *cmd)
{
    struct stat s;
    // must be accessible
    if (stat(cmd, &s) < 0)
        return 0;
    // must be a regular file
    //if (!(s.st_mode & S_IFREG))
    if (!S_ISREG(s.st_mode))
        return 0;
    // if it's owner executable by us, ok
    if (s.st_uid == getuid() && s.st_mode & S_IXUSR)
        return 1;
    // if it's group executable by us, ok
    if (s.st_gid == getgid() && s.st_mode & S_IXGRP)
        return 1;
    // if it's other executable by us, ok
    if (s.st_mode & S_IXOTH)
        return 1;
    return 0;
}

// tokenise: split a string around a set of separators
// create an array of separate strings
// final array element contains NULL
char **tokenise(char *str, char *sep)
{
    // temp copy of string, because strtok() mangles it
    char *tmp;
    // count tokens
    tmp = strdup(str);
    int n = 0;
    strtok(tmp, sep); n++;
    while (strtok(NULL, sep) != NULL) n++;
    free(tmp);
    // allocate array for argv strings
    char **strings = malloc((n+1)*sizeof(char *));
    assert(strings != NULL);
    // now tokenise and fill array
    tmp = strdup(str);
    char *next; int i = 0;
    next = strtok(tmp, sep);
    strings[i++] = strdup(next);
    while ((next = strtok(NULL,sep)) != NULL)
        strings[i++] = strdup(next);
    strings[i] = NULL;
    free(tmp);
    return strings;
}

// freeTokens: free memory associated with array of tokens
void freeTokens(char **toks)
{
    for (int i = 0; toks[i] != NULL; i++)
        free(toks[i]);
    free(toks);
}

// trim: remove leading/trailing spaces from a string
void trim(char *str)
{
    int first, last;
    first = 0;
    while (isspace(str[first])) first++;
    last  = strlen(str)-1;
    while (isspace(str[last])) last--;
    int i, j = 0;
    for (i = first; i <= last; i++) str[j++] = str[i];
    str[j] = '\0';
}

// strContains: does the first string contain any char from 2nd string?
int strContains(char *str, char *chars)
{
    for (char *s = str; *s != '\0'; s++) {
        for (char *c = chars; *c != '\0'; c++) {
            if (*s == *c) return 1;
        }
    }
    return 0;
}

// prompt: print a shell prompt
// done as a function to allow switching to $PS1
void prompt(void)
{
    printf("mymysh$ ");
}


char** fileNameExpand(char **tokens){

    char** expanded_line = malloc(MAXLINE * sizeof(tokens)) ;
    int arr_counter = 0;
    int wildcards_found = 0;

    for (int i = 0; tokens[i] != NULL ; i++){
        D(printf("Tokenised line = %s\n", tokens[i]));
        for(int j = 0 ; tokens[i][j] != '\0'; j++){
            //Go through each char of tokens[i][j]
            int ch = tokens[i][j] ;
            if(ch == '*' || ch == '?' || ch == '[' || ch == '~'){
                //expand the wildcard
                wildcards_found = 1;
                setenv("HOME", HOME_DIR, 1);
                wordexp_t wordexpbuf;
                int returnVal = wordexp(tokens[i], &wordexpbuf , 0);

                if(returnVal != 0){
                    printf("Wordexp error\n");
                }else{
                    for (int i = 0; i < wordexpbuf.we_wordc ; i++){
                        D(printf("Adding %s to index %d\n", wordexpbuf.we_wordv[i], arr_counter));
                        expanded_line[arr_counter] = wordexpbuf.we_wordv[i];
                        arr_counter++;
                    }
                    //wordfree(&wordexpbuf);
                }
                break;
            }
        }

        D(printf("Outside loops\n"));
        expanded_line[arr_counter] = tokens[i];
        D(printf("Adding %s to index %d\n", tokens[i], arr_counter));
        arr_counter++;
    }


    //Removing the last element which is the wildcard string
    if(arr_counter > 1 && wildcards_found == 1){
        expanded_line[arr_counter] = "";
        arr_counter--;
        D(printf("Array count after reduction = %d\n", arr_counter));
    }

    expanded_line[arr_counter++] = NULL;
    D(printf("NEW ARR:\n"));
    for(int i = 0 ; i < arr_counter ; i++){
        D(printf("%s ", expanded_line[i]));
    }
    D(printf("\n"));

    return expanded_line;
}

void hyphenate(){
    printf("--------------------\n");
}