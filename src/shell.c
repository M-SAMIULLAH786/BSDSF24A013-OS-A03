#include "shell.h"
#include <dirent.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>

static char** command_candidates;
static size_t candidate_count;
static size_t candidate_index;

/* Feature 8: variables are kept in a simple linked list. */
typedef struct ShellVariable {
    char* name;
    char* value;
    struct ShellVariable* next;
} ShellVariable;

static ShellVariable* shell_variables = NULL;

static int valid_variable_name(const char* name, size_t length) {
    if (length == 0 || !(isalpha((unsigned char)name[0]) || name[0] == '_'))
        return 0;
    for (size_t i = 1; i < length; i++) {
        if (!(isalnum((unsigned char)name[i]) || name[i] == '_')) return 0;
    }
    return 1;
}

static ShellVariable* find_shell_variable(const char* name, size_t length) {
    for (ShellVariable* item = shell_variables; item != NULL; item = item->next) {
        if (strlen(item->name) == length && strncmp(item->name, name, length) == 0)
            return item;
    }
    return NULL;
}

/* Returns 1 when the whole input is an assignment, 0 otherwise, -1 on error. */
int set_shell_variable_from_assignment(const char* line) {
    if (line == NULL) return 0;
    const char* equal = strchr(line, '=');
    if (equal == NULL) return 0;

    /* Assignment must be the complete command; its name cannot contain whitespace. */
    for (const char* p = line; p < equal; p++) {
        if (isspace((unsigned char)*p)) return 0;
    }
    size_t name_length = (size_t)(equal - line);
    if (!valid_variable_name(line, name_length)) {
        fprintf(stderr, "Invalid variable name in assignment.\n");
        return -1;
    }

    char* name = strndup(line, name_length);
    char* value = strdup(equal + 1);
    if (name == NULL || value == NULL) {
        perror("strdup");
        free(name);
        free(value);
        return -1;
    }

    ShellVariable* item = find_shell_variable(name, name_length);
    if (item != NULL) {
        free(item->value);
        item->value = value;
        free(name);
        return 1;
    }

    item = malloc(sizeof(*item));
    if (item == NULL) {
        perror("malloc");
        free(name);
        free(value);
        return -1;
    }
    item->name = name;
    item->value = value;
    item->next = shell_variables;
    shell_variables = item;
    return 1;
}

/* Expand $NAME references. Undefined variables expand to an empty string. */
char* expand_shell_variables(const char* input) {
    if (input == NULL) return NULL;
    size_t capacity = strlen(input) + 1;
    size_t length = 0;
    char* output = malloc(capacity);
    if (output == NULL) {
        perror("malloc");
        return NULL;
    }

    for (size_t i = 0; input[i] != '\0';) {
        if (input[i] == '$' &&
            (isalpha((unsigned char)input[i + 1]) || input[i + 1] == '_')) {
            size_t start = ++i;
            while (isalnum((unsigned char)input[i]) || input[i] == '_') i++;
            ShellVariable* item = find_shell_variable(input + start, i - start);
            const char* replacement = item == NULL ? "" : item->value;
            size_t replacement_length = strlen(replacement);
            if (length + replacement_length + 1 > capacity) {
                size_t new_capacity = capacity;
                while (length + replacement_length + 1 > new_capacity)
                    new_capacity *= 2;
                char* grown = realloc(output, new_capacity);
                if (grown == NULL) {
                    perror("realloc");
                    free(output);
                    return NULL;
                }
                output = grown;
                capacity = new_capacity;
            }
            memcpy(output + length, replacement, replacement_length);
            length += replacement_length;
        } else {
            if (length + 2 > capacity) {
                size_t new_capacity = capacity * 2;
                char* grown = realloc(output, new_capacity);
                if (grown == NULL) {
                    perror("realloc");
                    free(output);
                    return NULL;
                }
                output = grown;
                capacity = new_capacity;
            }
            output[length++] = input[i++];
        }
    }
    output[length] = '\0';
    return output;
}

void print_shell_variables(void) {
    if (shell_variables == NULL) {
        printf("No shell variables set.\n");
        return;
    }
    for (ShellVariable* item = shell_variables; item != NULL; item = item->next)
        printf("%s=%s\n", item->name, item->value);
}

static void clear_command_candidates(void) {
    for (size_t i = 0; i < candidate_count; i++) free(command_candidates[i]);
    free(command_candidates);
    command_candidates = NULL;
    candidate_count = candidate_index = 0;
}

static int candidate_exists(const char* name) {
    for (size_t i = 0; i < candidate_count; i++)
        if (strcmp(command_candidates[i], name) == 0) return 1;
    return 0;
}

static void add_candidate(const char* name) {
    if (name[0] == '\0' || candidate_exists(name)) return;
    char** grown = realloc(command_candidates, sizeof(char*) * (candidate_count + 1));
    if (grown == NULL) return;
    command_candidates = grown;
    command_candidates[candidate_count] = strdup(name);
    if (command_candidates[candidate_count] != NULL) candidate_count++;
}

static void collect_command_candidates(void) {
    static const char* builtins[] = {"cd", "exit", "help", "history", "jobs", "set", NULL};
    clear_command_candidates();
    for (int i = 0; builtins[i] != NULL; i++) add_candidate(builtins[i]);

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
                         directory[0] == '\0' ? "." : directory, entry->d_name);
                if (access(full_path, X_OK) == 0) add_candidate(entry->d_name);
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
        if (strncmp(candidate, text, length) == 0) return strdup(candidate);
    }
    return NULL;
}

static char** shell_completion(const char* text, int start, int end) {
    (void)end;
    if (start == 0) return rl_completion_matches(text, command_generator);
    return rl_completion_matches(text, rl_filename_completion_function);
}

char* read_cmd(char* prompt, FILE* fp) {
    (void)fp;
    rl_attempted_completion_function = shell_completion;
    return readline(prompt);
}

/* Recognize shell operators even when users do not surround them with spaces. */
char** tokenize(char* cmdline) {
    if (cmdline == NULL || cmdline[0] == '\0') return NULL;

    char** arglist = calloc(MAXARGS + 1, sizeof(char*));
    if (arglist == NULL) {
        perror("calloc");
        return NULL;
    }

    int argnum = 0;
    char* cp = cmdline;

    while (*cp != '\0') {
        while (*cp == ' ' || *cp == '\t' || *cp == '\n' || *cp == '\r') cp++;
        if (*cp == '\0') break;

        if (*cp == '<' || *cp == '>' || *cp == '|' || *cp == ';' || *cp == '&') {
            if (argnum >= MAXARGS) {
                fprintf(stderr, "Too many tokens (maximum %d).\n", MAXARGS);
                goto token_error;
            }
            arglist[argnum] = strndup(cp, 1);
            if (arglist[argnum++] == NULL) goto token_error;
            cp++;
            continue;
        }

        char* start = cp;
        while (*cp != '\0' && *cp != ' ' && *cp != '\t' && *cp != '\n' && *cp != '\r' &&
               *cp != '<' && *cp != '>' && *cp != '|' && *cp != ';' && *cp != '&')
            cp++;

        size_t length = (size_t)(cp - start);
        if (argnum >= MAXARGS) {
            fprintf(stderr, "Too many tokens (maximum %d).\n", MAXARGS);
            goto token_error;
        }
        if (length >= ARGLEN) {
            fprintf(stderr, "Argument is too long (maximum %d characters).\n", ARGLEN - 1);
            length = ARGLEN - 1;
        }
        arglist[argnum] = strndup(start, length);
        if (arglist[argnum++] == NULL) goto token_error;
    }

    if (argnum == 0) {
        free(arglist);
        return NULL;
    }
    arglist[argnum] = NULL;
    return arglist;

token_error:
    for (int i = 0; i < argnum; i++) free(arglist[i]);
    free(arglist);
    perror("tokenize");
    return NULL;
}
