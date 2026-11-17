#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

int main() {
    char* cmdline;
    char** arglist;

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

        // Split the line on ';' and run each piece one after another
        char* saveptr;
        for (char* seg = strtok_r(cmdline, ";", &saveptr);
             seg != NULL;
             seg = strtok_r(NULL, ";", &saveptr)) {

            if ((arglist = tokenize(seg)) == NULL) continue;

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
                    execute(arglist, background, seg);
                }
            }

            // Free the memory allocated by tokenize()
            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);
        }
        free(cmdline);
    }

    printf("\nShell exited.\n");
    return 0;
}
