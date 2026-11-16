#include "shell.h"

int execute(char* arglist[]) {
    command_t cmds[MAX_CMDS];
    int status;

    int ncmds = parse_pipeline(arglist, cmds);
    if (ncmds < 0) {
        return -1;                       // syntax error already printed
    }
    if (ncmds > 1) {
        fprintf(stderr, "pipes not implemented yet\n");
        return -1;
    }

    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            exit(1);
        case 0: // Child process
            if (cmds[0].infile != NULL) {
                int fd = open(cmds[0].infile, O_RDONLY);
                if (fd < 0) {
                    perror(cmds[0].infile);
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (cmds[0].outfile != NULL) {
                int fd = open(cmds[0].outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror(cmds[0].outfile);
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            execvp(cmds[0].argv[0], cmds[0].argv);
            perror("Command not found"); // This line runs only if execvp fails
            exit(1);
        default: // Parent process
            waitpid(cpid, &status, 0);
            return 0;
    }
}
