#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 100
#define MAX_CMDS 10
#define MAX_JOBS 20
#define PROMPT "FCIT> "
#define HISTORY_SIZE 20

// One command of a pipeline
typedef struct {
    char* argv[MAXARGS + 1];
    char* infile;     // target of '<', or NULL
    char* outfile;    // target of '>', or NULL
} command_t;

// Function prototypes
char* read_cmd(char* prompt, FILE* fp);
char** tokenize(char* cmdline);
int parse_pipeline(char** arglist, command_t cmds[]);
int execute(char** arglist, int background, const char* cmdline);
int handle_builtin(char** arglist);
void add_to_history(const char* cmdline);
char* get_history(int n);
void print_history(void);
void add_job(pid_t pid, const char* cmd);
void reap_jobs(void);
void print_jobs(void);
int try_assignment(char** arglist);
void expand_variables(char** arglist);
void print_vars(void);

#endif // SHELL_H
