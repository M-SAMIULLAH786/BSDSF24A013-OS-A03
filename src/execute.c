#include "shell.h"

/* Run shell built-in commands in the shell process itself. */
static int run_builtin(char* arglist[]) {
    if (strcmp(arglist[0], "exit") == 0) {
        return 1; /* Tell main() to leave the shell loop. */
    }

    if (strcmp(arglist[0], "cd") == 0) {
        const char* directory = arglist[1];

        if (directory == NULL) {
            directory = getenv("HOME");
            if (directory == NULL) {
                fprintf(stderr, "cd: HOME is not set\n");
                return 0;
            }
        }

        if (chdir(directory) != 0) {
            perror("cd");
        }
        return 0;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit              Exit the shell\n");
        printf("  cd <directory>    Change the current directory\n");
        printf("  help              Display this help message\n");
        printf("  jobs              Show background jobs (implemented in a later feature)\n");
        printf("\nOther commands are run as external programs.\n");
        return 0;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("No background jobs (job tracking will be added in a later feature).\n");
        return 0;
    }

    return -1; /* Not a built-in command. */
}

int execute(char* arglist[]) {
    int builtin_result = run_builtin(arglist);

    /* Built-ins must run before fork(), inside the shell process. */
    if (builtin_result != -1) {
        return builtin_result;
    }

    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            return -1;
        case 0: /* Child process */
            execvp(arglist[0], arglist);
            perror("Command not found");
            _exit(1);
        default: /* Parent process */
            if (waitpid(cpid, &status, 0) == -1) {
                perror("waitpid failed");
                return -1;
            }
            return 0;
    }
}
