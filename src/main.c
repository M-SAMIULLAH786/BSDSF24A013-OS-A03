#include "shell.h"
#include <readline/history.h>

#define HISTORY_LIMIT 20

static char* command_history[HISTORY_LIMIT];
static int history_count = 0;

/* Keep the custom !n history synchronized with Readline's arrow-key history. */
static void store_command_history(const char* command) {
    if (history_count == HISTORY_LIMIT) {
        free(command_history[0]);

        for (int i = 1; i < HISTORY_LIMIT; i++) {
            command_history[i - 1] = command_history[i];
        }
        history_count--;
    }

    command_history[history_count] = strdup(command);
    if (command_history[history_count] != NULL) {
        history_count++;
        add_history(command);
    }
}

static void print_history(void) {
    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, command_history[i]);
    }
}

/* Expand !n before tokenization and before storing the command. */
static int expand_history(char** command) {
    if ((*command)[0] != '!' || (*command)[1] == '\0') {
        return 0;
    }

    char* end;
    long number = strtol(*command + 1, &end, 10);

    if (*end != '\0') {
        return 0;
    }

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

static void free_history(void) {
    for (int i = 0; i < history_count; i++) {
        free(command_history[i]);
    }
}

int main(void) {
    char* cmdline;
    char** arglist;

    using_history();
    stifle_history(HISTORY_LIMIT);

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
        int history_result = expand_history(&cmdline);

        if (history_result == -1) {
            free(cmdline);
            continue;
        }

        if (cmdline[0] != '\0') {
            store_command_history(cmdline);
        }

        if (strcmp(cmdline, "history") == 0) {
            print_history();
            free(cmdline);
            continue;
        }

        if ((arglist = tokenize(cmdline)) != NULL) {
            int result = execute(arglist);

            for (int i = 0; arglist[i] != NULL; i++) {
                free(arglist[i]);
            }
            free(arglist);

            if (result == 1) {
                free(cmdline);
                break;
            }
        }
        free(cmdline);
    }

    free_history();
    clear_history();
    printf("\nShell exited.\n");
    return 0;
}
