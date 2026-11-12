#include "shell.h"
#include <fcntl.h>

#define MAX_JOBS 128

static pid_t background_pids[MAX_JOBS];
static int background_count = 0;

static void remove_background_pid(pid_t pid) {
    for (int i = 0; i < background_count; i++) {
        if (background_pids[i] == pid) {
            for (int j = i + 1; j < background_count; j++)
                background_pids[j - 1] = background_pids[j];
            background_count--;
            return;
        }
    }
}

/* Reap any completed child without blocking the shell. */
void reap_background_jobs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
        remove_background_pid(pid);
}

static void add_background_pid(pid_t pid) {
    if (background_count < MAX_JOBS) {
        background_pids[background_count++] = pid;
        printf("[background] started PID %ld\n", (long)pid);
    } else {
        fprintf(stderr, "Job list is full; waiting for PID %ld.\n", (long)pid);
        waitpid(pid, NULL, 0);
    }
}

static void list_background_jobs(void) {
    reap_background_jobs();
    if (background_count == 0) {
        printf("No background jobs.\n");
        return;
    }
    printf("Active background jobs:\n");
    for (int i = 0; i < background_count; i++)
        printf("  PID %ld\n", (long)background_pids[i]);
}

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
        printf("  jobs              List active background processes\n");
        printf("\nRedirection: command < input-file, command > output-file\n");
        printf("Pipe: command1 | command2\n");
        printf("Chaining: command1 ; command2\n");
        printf("Background: command &\n");
        return 0;
    }

    if (strcmp(arglist[0], "jobs") == 0) {
        list_background_jobs();
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

static int wait_for_child(pid_t pid) {
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        perror("waitpid");
        return -1;
    }
    return 0;
}

static int execute_single(ParsedCommand* command, int background) {
    pid_t pid = fork();
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

    if (background) {
        add_background_pid(pid);
        return 0;
    }
    return wait_for_child(pid);
}

/* Execute two commands connected by one pipe, foreground or background. */
static int execute_pipeline(ParsedCommand* left, ParsedCommand* right, int background) {
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
        wait_for_child(left_pid);
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

    close(pipe_fds[0]);
    close(pipe_fds[1]);
    if (background) {
        add_background_pid(left_pid);
        add_background_pid(right_pid);
        return 0;
    }
    wait_for_child(left_pid);
    wait_for_child(right_pid);
    return 0;
}

static int execute_segment(char** tokens, int start, int end, int background) {
    if (start >= end) {
        fprintf(stderr, "Syntax error: empty command.\n");
        return -1;
    }

    int pipe_position = -1;
    for (int i = start; i < end; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            if (pipe_position != -1) {
                fprintf(stderr, "Only one pipe (two commands) is supported.\n");
                return -1;
            }
            pipe_position = i;
        }
    }

    if (pipe_position == -1) {
        ParsedCommand command;
        if (parse_command(tokens, start, end, &command) != 0) return -1;

        if (!background && command.input_file == NULL && command.output_file == NULL) {
            int builtin_result = run_builtin(command.argv);
            if (builtin_result != -1) return builtin_result;
        }
        return execute_single(&command, background);
    }

    ParsedCommand left, right;
    if (parse_command(tokens, start, pipe_position, &left) != 0) return -1;
    if (parse_command(tokens, pipe_position + 1, end, &right) != 0) return -1;
    return execute_pipeline(&left, &right, background);
}


/* Execute a condition and choose the then/else command block from its exit status. */
int execute_if_block(const char* condition_text, const char* then_text, const char* else_text) {
    char* condition_copy = strdup(condition_text);
    if (condition_copy == NULL) {
        perror("strdup");
        return -1;
    }
    char** condition_args = tokenize(condition_copy);
    if (condition_args == NULL) {
        free(condition_copy);
        fprintf(stderr, "Syntax error: invalid if condition.\\n");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        for (int i = 0; condition_args[i] != NULL; i++) free(condition_args[i]);
        free(condition_args);
        free(condition_copy);
        return -1;
    }
    if (pid == 0) {
        execvp(condition_args[0], condition_args);
        perror(condition_args[0]);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        perror("waitpid");
        status = -1;
        break;
    }

    int condition_succeeded = status != -1 && WIFEXITED(status) &&
                              WEXITSTATUS(status) == 0;
    for (int i = 0; condition_args[i] != NULL; i++) free(condition_args[i]);
    free(condition_args);
    free(condition_copy);

    const char* selected = condition_succeeded ? then_text : else_text;
    if (selected == NULL || selected[0] == '\\0') return 0;

    char* commands_copy = strdup(selected);
    if (commands_copy == NULL) {
        perror("strdup");
        return -1;
    }
    char** commands = tokenize(commands_copy);
    int result = 0;
    if (commands != NULL) {
        result = execute(commands);
        for (int i = 0; commands[i] != NULL; i++) free(commands[i]);
        free(commands);
    }
    free(commands_copy);
    return result;
}

/* Split a line on ; and &, executing each segment in order. */
int execute(char* arglist[]) {
    int start = 0;
    int count = 0;
    while (arglist[count] != NULL) count++;

    for (int i = 0; i <= count; i++) {
        if (i == count || strcmp(arglist[i], ";") == 0 ||
            strcmp(arglist[i], "&") == 0) {
            int background = (i < count && strcmp(arglist[i], "&") == 0);
            if (i > start) {
                int result = execute_segment(arglist, start, i, background);
                if (result == 1) return 1;
                if (result < 0) return result;
            } else if (i < count) {
                fprintf(stderr, "Syntax error: empty command.\n");
                return -1;
            }
            start = i + 1;
        }
    }
    return 0;
}
