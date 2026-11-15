#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {

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
            add_to_history(cmdline);
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            if (handle_builtin(arglist) == 0) {
                execute(arglist);
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
