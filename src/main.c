#include "shell.h"
#include <readline/history.h>
#include <readline/readline.h>
#define HISTORY_LIMIT 20

static char* command_history[HISTORY_LIMIT];
static int history_count = 0;

static void store_command_history(const char* command) {
    if (history_count == HISTORY_LIMIT) {
        free(command_history[0]);
        for (int i = 1; i < HISTORY_LIMIT; i++)
            command_history[i - 1] = command_history[i];
        history_count--;
    }

    command_history[history_count] = strdup(command);
    if (command_history[history_count] != NULL) {
        history_count++;
        add_history(command);
    }
}

static void print_history(void) {
    for (int i = 0; i < history_count; i++)
        printf("%d  %s\n", i + 1, command_history[i]);
}

static int expand_history(char** command) {
    if ((*command)[0] != '!' || (*command)[1] == '\0') return 0;

    char* end;
    long number = strtol(*command + 1, &end, 10);
    if (*end != '\0') return 0;

    if (number < 1 || number > history_count) {
        fprintf(stderr, "History: command number %ld does not exist.\n", number);
        return -1;
    }

    char* recalled = strdup(command_history[number - 1]);
    if (recalled == NULL) {
        perror("strdup");
        return -1;
    }

    free(*command);
    *command = recalled;
    printf("%s\n", *command);
    return 1;
}

static char* append_line(char* text, const char* line) {
    size_t old_length = text == NULL ? 0 : strlen(text);
    size_t line_length = strlen(line);
    char* grown = realloc(text, old_length + line_length + 2);
    if (grown == NULL) {
        perror("realloc");
        free(text);
        return NULL;
    }
    memcpy(grown + old_length, line, line_length);
    grown[old_length + line_length] = ';';
    grown[old_length + line_length + 1] = '\0';
    return grown;
}

static char* trim_line(char* line) {
    while (*line == ' ' || *line == '\t') line++;
    size_t length = strlen(line);
    while (length > 0 && (line[length - 1] == ' ' || line[length - 1] == '\t' ||
                          line[length - 1] == '\r' || line[length - 1] == '\n'))
        line[--length] = '\0';
    return line;
}

/* Read and execute a multiline if/then/else/fi block. */
static int read_if_block(const char* first_line) {
    const char* condition_start = first_line + 2;
    while (*condition_start == ' ' || *condition_start == '\t') condition_start++;
    if (*condition_start == '\0') {
        fprintf(stderr, "Syntax error: if requires a condition.\n");
        return 0;
    }

    char* condition = strdup(trim_line((char*)condition_start));
    char* then_commands = NULL;
    char* else_commands = NULL;
    char* input = NULL;
    int in_then = 0, in_else = 0, got_then = 0, got_fi = 0;

    while ((input = readline("... ")) != NULL) {
        char* line = trim_line(input);
        if (*line == '\0') { free(input); continue; }
        if (strcmp(line, "then") == 0) {
            if (got_then) {
                fprintf(stderr, "Syntax error: unexpected then.\n");
                free(input); break;
            }
            got_then = 1;
            in_then = 1;
        } else if (strcmp(line, "else") == 0) {
            if (!got_then || in_else) {
                fprintf(stderr, "Syntax error: unexpected else.\n");
                free(input); break;
            }
            in_then = 0;
            in_else = 1;
        } else if (strcmp(line, "fi") == 0) {
            got_fi = 1;
            free(input);
            break;
        } else if (!got_then) {
            fprintf(stderr, "Syntax error: expected then.\n");
            free(input); break;
        } else if (in_then) {
            then_commands = append_line(then_commands, line);
        } else if (in_else) {
            else_commands = append_line(else_commands, line);
        } else {
            fprintf(stderr, "Syntax error: unexpected command in conditional block.\n");
            free(input); break;
        }
        free(input);
    }

    int result = 0;
    if (!got_then || !got_fi) {
        fprintf(stderr, "Syntax error: conditional block must contain then and fi.\n");
    } else {
        result = execute_if_block(condition, then_commands, else_commands);
    }
    free(condition);
    free(then_commands);
    free(else_commands);
    return result;
}

static void free_history(void) {
    for (int i = 0; i < history_count; i++) free(command_history[i]);
}

int main(void) {
    char* cmdline;
    char** arglist;

    using_history();
    stifle_history(HISTORY_LIMIT);

    while (1) {
        reap_background_jobs();

        cmdline = read_cmd(PROMPT, stdin);
        if (cmdline == NULL) break;

        int history_result = expand_history(&cmdline);
        if (history_result == -1) {
            free(cmdline);
            continue;
        }

        if (cmdline[0] != '\0') store_command_history(cmdline);

        if (strncmp(cmdline, "if", 2) == 0 &&
            (cmdline[2] == ' ' || cmdline[2] == '\t')) {
            read_if_block(cmdline);
            free(cmdline);
            continue;
        }

        if (strcmp(cmdline, "history") == 0) {
            print_history();
            free(cmdline);
            continue;
        }

        int assignment_result = set_shell_variable_from_assignment(cmdline);
        if (assignment_result != 0) {
            free(cmdline);
            continue;
        }

        char* expanded_line = expand_shell_variables(cmdline);
        if (expanded_line == NULL) {
            free(cmdline);
            continue;
        }
        arglist = tokenize(expanded_line);
        if (arglist != NULL) {
            int result = execute(arglist);
            for (int i = 0; arglist[i] != NULL; i++) free(arglist[i]);
            free(arglist);
            if (result == 1) {
                free(expanded_line);
                free(cmdline);
                break;
            }
        }
        free(expanded_line);
        free(cmdline);
    }

    free_history();
    clear_history();
    printf("\nShell exited.\n");
    return 0;
}
