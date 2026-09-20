#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "executor.h"

int execute_external(Command *command) {
    if (command->argc == 0) {
        return 0;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execvp(command->argv[0], command->argv);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    int status = 0;

    if (!command->background) {
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return 1;
        }
    } else {
        printf("[background pid %d]\n", pid);
    }

    return status;
}
