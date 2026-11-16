#include "shell.h"

static char* history[HISTORY_SIZE];
static int history_count = 0;

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    char* cmdline = (char*) malloc(sizeof(char) * MAX_LEN);
    int c, pos = 0;

    while ((c = getc(fp)) != EOF) {
        if (c == '\n') break;
        cmdline[pos++] = c;
    }

    if (c == EOF && pos == 0) {
        free(cmdline);
        return NULL; // Handle Ctrl+D
    }
    
    cmdline[pos] = '\0';
    return cmdline;
}

static int is_operator(char c) {
    return c == '<' || c == '>' || c == '|';
}

static int is_operator_token(const char* s) {
    return strcmp(s, "<") == 0 || strcmp(s, ">") == 0 || strcmp(s, "|") == 0;
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        if (is_operator(*cp)) {
            cp++;                               // operator is its own token
        } else {
            while (*cp != '\0' && *cp != ' ' && *cp != '\t' && !is_operator(*cp)) {
                cp++;
            }
        }

        len = cp - start;
        if (len > ARGLEN - 1) len = ARGLEN - 1; // don't overflow the buffer
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for(int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    // free the unused token buffers, then terminate the list
    for (int i = argnum; i < MAXARGS + 1; i++) {
        free(arglist[i]);
    }
    arglist[argnum] = NULL;
    return arglist;
}

// Splits tokens into commands at '|', and picks out '<' and '>' targets.
// Returns the number of commands, or -1 on a syntax error.
int parse_pipeline(char** arglist, command_t cmds[]) {
    int n = 0;       // index of current command
    int argc = 0;    // args collected for current command

    memset(&cmds[0], 0, sizeof(command_t));

    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            if (argc == 0) {
                fprintf(stderr, "syntax error near '|'\n");
                return -1;
            }
            cmds[n].argv[argc] = NULL;
            n++;
            memset(&cmds[n], 0, sizeof(command_t));
            argc = 0;
        } else if (strcmp(arglist[i], "<") == 0 || strcmp(arglist[i], ">") == 0) {
            int is_input = (arglist[i][0] == '<');
            if (arglist[i + 1] == NULL || is_operator_token(arglist[i + 1])) {
                fprintf(stderr, "syntax error: missing file after '%s'\n", arglist[i]);
                return -1;
            }
            if (is_input) {
                cmds[n].infile = arglist[i + 1];
            } else {
                cmds[n].outfile = arglist[i + 1];
            }
            i++;                                // skip the filename
        } else {
            cmds[n].argv[argc++] = arglist[i];
        }
    }

    if (argc == 0) {
        fprintf(stderr, "syntax error: missing command\n");
        return -1;
    }
    cmds[n].argv[argc] = NULL;
    return n + 1;
}

void add_to_history(const char* cmdline) {
    if (history_count == HISTORY_SIZE) {
        free(history[0]);                       // drop the oldest
        memmove(history, history + 1, (HISTORY_SIZE - 1) * sizeof(char*));
        history_count--;
    }
    history[history_count++] = strdup(cmdline);
}

char* get_history(int n) {
    if (n < 1 || n > history_count) {
        return NULL;
    }
    return history[n - 1];
}

void print_history(void) {
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }
}

int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
    }

    if (strcmp(arglist[0], "exit") == 0) {
        printf("Shell exited.\n");
        exit(0);
    }

    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL) {
            fprintf(stderr, "cd: missing directory\n");
        } else if (chdir(arglist[1]) != 0) {
            perror("cd failed");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit         - exit the shell\n");
        printf("  cd <dir>     - change directory\n");
        printf("  help         - show this message\n");
        printf("  jobs         - list background jobs (not implemented yet)\n");
        printf("  history      - show command history\n");
        printf("  !n           - re-run the n-th command from history\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    return 0;
}
