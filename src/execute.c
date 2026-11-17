#include "shell.h"

int execute(char* arglist[], int background, const char* cmdline) {
    command_t cmds[MAX_CMDS];
    pid_t pids[MAX_CMDS];
    int status;

    int ncmds = parse_pipeline(arglist, cmds);
    if (ncmds < 0) {
        return -1;                       // syntax error already printed
    }

    int prev_read = -1;                  // read end of the previous pipe
    int fds[2];

    for (int i = 0; i < ncmds; i++) {
        // create a pipe unless this is the last command
        if (i < ncmds - 1) {
            if (pipe(fds) < 0) {
                perror("pipe failed");
                return -1;
            }
        }

        int cpid = fork();
        if (cpid < 0) {
            perror("fork failed");
            exit(1);
        }

        if (cpid == 0) { // Child process
            // connect stdin to previous pipe
            if (prev_read != -1) {
                dup2(prev_read, STDIN_FILENO);
                close(prev_read);
            }
            // connect stdout to next pipe
            if (i < ncmds - 1) {
                close(fds[0]);           // child doesn't read from its own pipe
                dup2(fds[1], STDOUT_FILENO);
                close(fds[1]);
            }
            // file redirections (override the pipe if both are given)
            if (cmds[i].infile != NULL) {
                int fd = open(cmds[i].infile, O_RDONLY);
                if (fd < 0) {
                    perror(cmds[i].infile);
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (cmds[i].outfile != NULL) {
                int fd = open(cmds[i].outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror(cmds[i].outfile);
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror("Command not found"); // This line runs only if execvp fails
            exit(1);
        }

        // Parent process: close ends it no longer needs
        pids[i] = cpid;
        if (prev_read != -1) {
            close(prev_read);
        }
        if (i < ncmds - 1) {
            close(fds[1]);
            prev_read = fds[0];          // next command reads from here
        }
    }

    if (background) {
        // don't wait: remember the last child and return to the prompt
        add_job(pids[ncmds - 1], cmdline);
        printf("[bg] started pid %d\n", (int)pids[ncmds - 1]);
    } else {
        // wait for every child in this pipeline
        for (int i = 0; i < ncmds; i++) {
            waitpid(pids[i], &status, 0);
        }
    }
    return 0;
}
