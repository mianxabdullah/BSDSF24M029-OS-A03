#include "shell.h"

static char* history[HISTORY_SIZE];
static int history_count = 0;

typedef struct {
    pid_t pid;
    char cmd[MAX_LEN];
} job_t;

static job_t jobs[MAX_JOBS];
static int job_count = 0;

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
    return c == '<' || c == '>' || c == '|' || c == '&';
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
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace

        if (*cp == '\0') break; // Line was only whitespace

        char* tok = arglist[argnum];
        int len = 0;

        if (is_operator(*cp)) {
            tok[len++] = *cp++;                 // operator is its own token
        } else {
            char quote = 0;                     // 0 = not inside quotes
            while (*cp != '\0') {
                if (quote) {
                    if (*cp == quote) {         // closing quote
                        quote = 0;
                        cp++;
                        continue;
                    }
                } else {
                    if (*cp == '"' || *cp == '\'') {   // opening quote
                        quote = *cp;
                        cp++;
                        continue;
                    }
                    if (*cp == ' ' || *cp == '\t' || is_operator(*cp)) {
                        break;                  // end of token
                    }
                }
                if (len < ARGLEN - 1) tok[len++] = *cp;
                cp++;
            }
        }
        tok[len] = '\0';
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

void add_job(pid_t pid, const char* cmd) {
    if (job_count == MAX_JOBS) {
        fprintf(stderr, "too many background jobs\n");
        return;
    }
    jobs[job_count].pid = pid;
    strncpy(jobs[job_count].cmd, cmd, MAX_LEN - 1);
    jobs[job_count].cmd[MAX_LEN - 1] = '\0';
    job_count++;
}

static void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            printf("[Done] %d %s\n", (int)pid, jobs[i].cmd);
            for (int j = i; j < job_count - 1; j++) {
                jobs[j] = jobs[j + 1];
            }
            job_count--;
            return;
        }
    }
}

// Collect finished children without blocking (prevents zombies)
void reap_jobs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_job(pid);
    }
}

void print_jobs(void) {
    if (job_count == 0) {
        printf("No background jobs.\n");
        return;
    }
    for (int i = 0; i < job_count; i++) {
        printf("[%d] %d  %s\n", i + 1, (int)jobs[i].pid, jobs[i].cmd);
    }
}

// ---------- shell variables (linked list) ----------

typedef struct var {
    char* name;
    char* value;
    struct var* next;
} var_t;

static var_t* vars = NULL;

static const char* get_var(const char* name) {
    for (var_t* v = vars; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) return v->value;
    }
    return NULL;
}

static void set_var(const char* name, const char* value) {
    // update if it already exists
    for (var_t* v = vars; v != NULL; v = v->next) {
        if (strcmp(v->name, name) == 0) {
            free(v->value);
            v->value = strdup(value);
            return;
        }
    }
    // otherwise append a new node at the end
    var_t* node = (var_t*)malloc(sizeof(var_t));
    node->name = strdup(name);
    node->value = strdup(value);
    node->next = NULL;

    if (vars == NULL) {
        vars = node;
    } else {
        var_t* last = vars;
        while (last->next != NULL) last = last->next;
        last->next = node;
    }
}

void print_vars(void) {
    for (var_t* v = vars; v != NULL; v = v->next) {
        printf("%s=%s\n", v->name, v->value);
    }
}

// If the line is a single token NAME=value, stores it and returns 1.
int try_assignment(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL || arglist[1] != NULL) return 0;

    char* eq = strchr(arglist[0], '=');
    if (eq == NULL || eq == arglist[0]) return 0;   // no '=' or empty name

    // name must be letters, digits, underscore and not start with a digit
    for (char* p = arglist[0]; p < eq; p++) {
        int ok = (*p == '_') ||
                 (*p >= 'a' && *p <= 'z') ||
                 (*p >= 'A' && *p <= 'Z') ||
                 (p != arglist[0] && *p >= '0' && *p <= '9');
        if (!ok) return 0;
    }

    *eq = '\0';                                      // split name and value
    set_var(arglist[0], eq + 1);
    return 1;
}

// Replaces every "$NAME" argument with the variable's value.
// Undefined variables are removed from the list.
void expand_variables(char** arglist) {
    int j = 0;
    for (int i = 0; arglist[i] != NULL; i++) {
        if (arglist[i][0] == '$' && arglist[i][1] != '\0') {
            const char* val = get_var(arglist[i] + 1);
            char* copy = (val != NULL) ? strdup(val) : NULL;
            free(arglist[i]);
            if (copy != NULL) arglist[j++] = copy;
        } else {
            arglist[j++] = arglist[i];
        }
    }
    arglist[j] = NULL;
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
        const char* target = arglist[1];
        if (target == NULL) {
            target = getenv("HOME");        // plain 'cd' goes home
            if (target == NULL) {
                fprintf(stderr, "cd: HOME not set\n");
                return 1;
            }
        }
        if (chdir(target) != 0) {
            perror("cd failed");
        }
        return 1;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit         - exit the shell\n");
        printf("  cd [dir]     - change directory (no argument = home)\n");
        printf("  help         - show this message\n");
        printf("  jobs         - list background jobs\n");
        printf("  history      - show command history\n");
        printf("  !n           - re-run the n-th command from history\n");
        printf("  set          - show all shell variables\n");
        printf("  NAME=value   - assign a shell variable ($NAME to use it)\n");
        return 1;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }

    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    if (strcmp(arglist[0], "set") == 0) {
        print_vars();
        return 1;
    }

    return 0;
}
