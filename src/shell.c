#include "shell.h"
#include <dirent.h>
#include <readline/readline.h>
#include <readline/history.h>

static char** command_candidates;
static size_t candidate_count;
static size_t candidate_index;

static void clear_command_candidates(void) {
    for (size_t i = 0; i < candidate_count; i++) {
        free(command_candidates[i]);
    }
    free(command_candidates);
    command_candidates = NULL;
    candidate_count = 0;
    candidate_index = 0;
}

static int candidate_exists(const char* name) {
    for (size_t i = 0; i < candidate_count; i++) {
        if (strcmp(command_candidates[i], name) == 0) return 1;
    }
    return 0;
}

static void add_candidate(const char* name) {
    if (name[0] == '\0' || candidate_exists(name)) return;

    char** grown = realloc(command_candidates,
                           sizeof(char*) * (candidate_count + 1));
    if (grown == NULL) return;

    command_candidates = grown;
    command_candidates[candidate_count] = strdup(name);
    if (command_candidates[candidate_count] != NULL) candidate_count++;
}

/* Collect built-ins and executable names found in directories listed in PATH. */
static void collect_command_candidates(void) {
    static const char* builtins[] = {
        "cd", "exit", "help", "history", "jobs", NULL
    };

    clear_command_candidates();

    for (int i = 0; builtins[i] != NULL; i++) {
        add_candidate(builtins[i]);
    }

    const char* path_value = getenv("PATH");
    if (path_value == NULL) return;

    char* path_copy = strdup(path_value);
    if (path_copy == NULL) return;

    char* save_pointer = NULL;
    char* directory = strtok_r(path_copy, ":", &save_pointer);

    while (directory != NULL) {
        DIR* folder = opendir(directory[0] == '\0' ? "." : directory);
        if (folder != NULL) {
            struct dirent* entry;
            while ((entry = readdir(folder)) != NULL) {
                size_t needed = strlen(directory) + strlen(entry->d_name) + 2;
                char* full_path = malloc(needed);
                if (full_path == NULL) continue;

                snprintf(full_path, needed, "%s/%s",
                         directory[0] == '\0' ? "." : directory,
                         entry->d_name);

                if (access(full_path, X_OK) == 0 &&
                    !candidate_exists(entry->d_name)) {
                    add_candidate(entry->d_name);
                }
                free(full_path);
            }
            closedir(folder);
        }
        directory = strtok_r(NULL, ":", &save_pointer);
    }

    free(path_copy);
}

static char* command_generator(const char* text, int state) {
    if (state == 0) {
        collect_command_candidates();
        candidate_index = 0;
    }

    size_t length = strlen(text);
    while (candidate_index < candidate_count) {
        const char* candidate = command_candidates[candidate_index++];
        if (strncmp(candidate, text, length) == 0) {
            return strdup(candidate);
        }
    }

    return NULL;
}

/* Complete commands from PATH at the start, and paths after a space. */
static char** shell_completion(const char* text, int start, int end) {
    (void)end;

    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }

    return rl_completion_matches(text, rl_filename_completion_function);
}

char* read_cmd(char* prompt, FILE* fp) {
    (void)fp;
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
