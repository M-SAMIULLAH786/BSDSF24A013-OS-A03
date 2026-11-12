#include "shell.h"
#include <fcntl.h>

/* Run shell built-ins in the parent shell process. */
static int run_builtin(char* arglist[]) {
    if (strcmp(arglist[0], "exit") == 0) return 1;

    if (strcmp(arglist[0], "cd") == 0) {
        const char* directory = arglist[1];
        if (directory == NULL) directory = getenv("HOME");
        if (directory == NULL) {
            fprintf(stderr, "cd: HOME is not set\n");
            return 0;
        }
        if (chdir(directory) != 0) perror("cd");
        return 0;
    }

    if (strcmp(arglist[0], "help") == 0) {
        printf("Built-in commands:\n");
        printf("  exit              Exit the shell\n");
        printf("  cd <directory>    Change the current directory\n");
        printf("  help              Display this help message\n");
        printf("  jobs              Show background jobs (later feature)\n");
        printf("\nRedirection: command < input-file, command > output-file\n");
        printf("Pipe: command1 | command2\n");
        return 0;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        printf("No background jobs (job tracking will be added later).\n");
        return 0;
    }
    return -1;
}

typedef struct {
    char* argv[MAXARGS + 1];
    char* input_file;
    char* output_file;
    int argc;
} ParsedCommand;

/* Separate ordinary arguments from input/output redirection operators. */
static int parse_command(char** tokens, int start, int end, ParsedCommand* parsed) {
    memset(parsed, 0, sizeof(*parsed));

    for (int i = start; i < end; i++) {
        if (strcmp(tokens[i], "<") == 0 || strcmp(tokens[i], ">") == 0) {
            int is_input = strcmp(tokens[i], "<") == 0;
            if (i + 1 >= end || strcmp(tokens[i + 1], "<") == 0 ||
                strcmp(tokens[i + 1], ">") == 0 || strcmp(tokens[i + 1], "|") == 0) {
                fprintf(stderr, "Syntax error: redirection requires a filename.\n");
                return -1;
            }
            if (is_input) {
                if (parsed->input_file != NULL) {
                    fprintf(stderr, "Syntax error: duplicate input redirection.\n");
                    return -1;
                }
                parsed->input_file = tokens[++i];
            } else {
                if (parsed->output_file != NULL) {
                    fprintf(stderr, "Syntax error: duplicate output redirection.\n");
                    return -1;
                }
                parsed->output_file = tokens[++i];
            }
        } else {
            if (parsed->argc >= MAXARGS) {
                fprintf(stderr, "Too many command arguments.\n");
                return -1;
            }
            parsed->argv[parsed->argc++] = tokens[i];
        }
    }
    parsed->argv[parsed->argc] = NULL;

    if (parsed->argc == 0) {
        fprintf(stderr, "Syntax error: missing command.\n");
        return -1;
    }
    return 0;
}

/* Connect requested files to standard input/output in a child process. */
static int apply_redirections(const ParsedCommand* command) {
    if (command->input_file != NULL) {
        int fd = open(command->input_file, O_RDONLY);
        if (fd < 0) {
            perror(command->input_file);
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2 input");
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (command->output_file != NULL) {
        int fd = open(command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror(command->output_file);
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 output");
            close(fd);
            return -1;
        }
        close(fd);
    }
    return 0;
}

static int execute_single(ParsedCommand* command) {
    int pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        if (apply_redirections(command) != 0) _exit(1);
        execvp(command->argv[0], command->argv);
        perror(command->argv[0]);
        _exit(127);
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }
    return 0;
}

/* Execute exactly two commands connected by one pipe. */
static int execute_pipeline(ParsedCommand* left, ParsedCommand* right) {
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
        perror("pipe");
        return -1;
    }

    pid_t left_pid = fork();
    if (left_pid < 0) {
        perror("fork");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return -1;
    }

    if (left_pid == 0) {
        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) {
            perror("dup2 pipe output");
            _exit(1);
        }
        close(pipe_fds[1]);
        if (apply_redirections(left) != 0) _exit(1);
        execvp(left->argv[0], left->argv);
        perror(left->argv[0]);
        _exit(127);
    }

    pid_t right_pid = fork();
    if (right_pid < 0) {
        perror("fork");
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        waitpid(left_pid, NULL, 0);
        return -1;
    }

    if (right_pid == 0) {
        close(pipe_fds[1]);
        if (dup2(pipe_fds[0], STDIN_FILENO) < 0) {
            perror("dup2 pipe input");
            _exit(1);
        }
        close(pipe_fds[0]);
        if (apply_redirections(right) != 0) _exit(1);
        execvp(right->argv[0], right->argv);
        perror(right->argv[0]);
        _exit(127);
    }

    /* Parent must close both ends so readers receive EOF correctly. */
    close(pipe_fds[0]);
    close(pipe_fds[1]);

    int left_status, right_status;
    if (waitpid(left_pid, &left_status, 0) < 0) perror("waitpid left");
    if (waitpid(right_pid, &right_status, 0) < 0) perror("waitpid right");
    return 0;
}

int execute(char* arglist[]) {
    int pipe_position = -1;

    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "|") == 0) {
            if (pipe_position != -1) {
                fprintf(stderr, "Only one pipe (two commands) is supported in this feature.\n");
                return -1;
            }
            pipe_position = i;
        }
    }

    if (pipe_position == -1) {
        ParsedCommand command;
        int count = 0;
        while (arglist[count] != NULL) count++;
        if (parse_command(arglist, 0, count, &command) != 0) return -1;

        /* Built-ins without redirection execute in the parent shell. */
        if (command.input_file == NULL && command.output_file == NULL) {
            int builtin_result = run_builtin(command.argv);
            if (builtin_result != -1) return builtin_result;
        }
        return execute_single(&command);
    }

    ParsedCommand left, right;
    if (parse_command(arglist, 0, pipe_position, &left) != 0) return -1;

    int count = pipe_position + 1;
    while (arglist[count] != NULL) count++;
    if (parse_command(arglist, pipe_position + 1, count, &right) != 0) return -1;

    return execute_pipeline(&left, &right);
}
