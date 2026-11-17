#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_BLOCK 20

static void free_args(char** arglist) {
    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);
}

// Runs one line (may contain ';' and '&').
// Returns the exit status of the last command run.
static int run_line(char* line) {
    int status = 0;
    char* saveptr;

    for (char* seg = strtok_r(line, ";", &saveptr);
         seg != NULL;
         seg = strtok_r(NULL, ";", &saveptr)) {

        char** arglist = tokenize(seg);
        if (arglist == NULL) continue;

        // NAME=value is a built-in action, no fork
        if (try_assignment(arglist)) {
            status = 0;
            free_args(arglist);
            continue;
        }

        // replace $NAME with its value
        expand_variables(arglist);
        if (arglist[0] == NULL) {       // everything expanded to nothing
            free(arglist);
            continue;
        }

        // A trailing '&' means run in the background
        int background = 0;
        int n = 0;
        while (arglist[n] != NULL) n++;
        if (strcmp(arglist[n - 1], "&") == 0) {
            free(arglist[n - 1]);
            arglist[n - 1] = NULL;
            background = 1;
            n--;
        }

        if (n > 0) {
            if (handle_builtin(arglist) == 0) {
                status = execute(arglist, background, seg);
            } else {
                status = 0;             // built-ins count as success
            }
        }

        free_args(arglist);
    }
    return status;
}

// If line starts with the word `word`, returns a pointer to what follows it
// (leading spaces skipped). Otherwise returns NULL.
static const char* after_word(const char* line, const char* word) {
    while (*line == ' ' || *line == '\t') line++;
    size_t n = strlen(word);
    if (strncmp(line, word, n) != 0) return NULL;
    if (line[n] != '\0' && line[n] != ' ' && line[n] != '\t') return NULL;
    line += n;
    while (*line == ' ' || *line == '\t') line++;
    return line;
}

static void add_line(char* block[], int* count, const char* line) {
    if (*count == MAX_BLOCK) {
        fprintf(stderr, "if block too long, line ignored\n");
        return;
    }
    block[(*count)++] = strdup(line);
}

// Reads the rest of an if-block, then runs the chosen branch.
static void handle_if(const char* cond_text) {
    char* cond = strdup(cond_text);
    char* then_block[MAX_BLOCK];
    char* else_block[MAX_BLOCK];
    int then_n = 0, else_n = 0;
    int state = 0;                      // 0 = waiting for then, 1 = then, 2 = else
    int closed = 0;                     // 1 once 'fi' was seen
    char* line;

    while ((line = readline("> ")) != NULL) {
        const char* rest;

        if (after_word(line, "fi") != NULL) {
            closed = 1;
            free(line);
            break;
        }
        if (state == 0 && (rest = after_word(line, "then")) != NULL) {
            state = 1;
            if (*rest) add_line(then_block, &then_n, rest);
        } else if (state == 1 && (rest = after_word(line, "else")) != NULL) {
            state = 2;
            if (*rest) add_line(else_block, &else_n, rest);
        } else if (state == 1) {
            add_line(then_block, &then_n, line);
        } else if (state == 2) {
            add_line(else_block, &else_n, line);
        }
        free(line);
    }

    if (!closed) {
        fprintf(stderr, "syntax error: missing 'fi'\n");
    } else if (state == 0) {
        fprintf(stderr, "syntax error: missing 'then'\n");
    } else {
        int status = run_line(cond);
        if (status == 0) {
            for (int i = 0; i < then_n; i++) run_line(then_block[i]);
        } else {
            for (int i = 0; i < else_n; i++) run_line(else_block[i]);
        }
    }

    free(cond);
    for (int i = 0; i < then_n; i++) free(then_block[i]);
    for (int i = 0; i < else_n; i++) free(else_block[i]);
}

int main() {
    char* cmdline;

    while (1) {
        reap_jobs();                    // collect finished background jobs

        cmdline = readline(PROMPT);
        if (cmdline == NULL) break;     // Ctrl+D

        // Handle !n before tokenizing and before adding to history
        if (cmdline[0] == '!') {
            char* old = get_history(atoi(cmdline + 1));
            if (old == NULL) {
                fprintf(stderr, "%s: event not found\n", cmdline);
                free(cmdline);
                continue;
            }
            free(cmdline);
            cmdline = strdup(old);
            printf("%s\n", cmdline);   // show the command being re-run
        }

        if (cmdline[0] != '\0') {
            add_history(cmdline);       // readline history (arrow keys)
            add_to_history(cmdline);    // our own history (history, !n)
        }

        // if-then-else-fi block
        const char* rest = after_word(cmdline, "if");
        if (rest != NULL) {
            handle_if(rest);
            free(cmdline);
            continue;
        }

        run_line(cmdline);
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
