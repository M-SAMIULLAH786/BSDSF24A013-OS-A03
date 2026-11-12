#include "shell.h"

int main() {
    char* cmdline;
    char** arglist;

    while ((cmdline = read_cmd(PROMPT, stdin)) != NULL) {
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

    printf("\nShell exited.\n");
    return 0;
}
