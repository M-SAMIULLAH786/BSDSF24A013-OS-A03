#include "shell.h"
#include <readline/readline.h>
#include <readline/history.h>

/* Built-in names offered when completing the first word. */
static const char* builtin_names[] = {
    "cd", "exit", "help", "history", "jobs", NULL
};

static char* command_generator(const char* text, int state) {
    static int index;
    size_t length;

    if (state == 0) {
        index = 0;
    }

    length = strlen(text);

    while (builtin_names[index] != NULL) {
        const char* name = builtin_names[index++];
        if (strncmp(name, text, length) == 0) {
            return strdup(name);
        }
    }

    return NULL;
}

/* Complete built-in commands at the start; complete paths after a space. */
static char** shell_completion(const char* text, int start, int end) {
    (void)end;

    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }

    return rl_completion_matches(text, rl_filename_completion_function);
}

char* read_cmd(char* prompt, FILE* fp) {
    (void)fp; /* readline reads from the interactive terminal. */
    rl_attempted_completion_function = shell_completion;
    return readline(prompt);
}

char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    if (arglist == NULL) {
        perror("malloc");
        return NULL;
    }

    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)calloc(ARGLEN, sizeof(char));
        if (arglist[i] == NULL) {
            for (int j = 0; j < i; j++) free(arglist[j]);
            free(arglist);
            perror("calloc");
            return NULL;
        }
    }

    char* cp = cmdline;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++;
        if (*cp == '\0') break;

        char* start = cp;
        size_t len = 0;
        while (*cp != '\0' && *cp != ' ' && *cp != '\t') {
            cp++;
            len++;
        }

        if (len >= ARGLEN) len = ARGLEN - 1;
        memcpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) {
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}
