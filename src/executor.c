#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "executor.h"

static void apply_redirection(Command *command) {
    if (command->input != NULL) {
        int input_fd = open(command->input, O_RDONLY);
        if (input_fd < 0) {
            perror(command->input);
            exit(EXIT_FAILURE);
        }

        if (dup2(input_fd, STDIN_FILENO) < 0) {
            perror("dup2");
            close(input_fd);
            exit(EXIT_FAILURE);
        }

        close(input_fd);
    }

    if (command->output != NULL) {
        int flags = O_WRONLY | O_CREAT;

        if (command->append) {
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }

        int output_fd = open(command->output, flags, 0644);
        if (output_fd < 0) {
            perror(command->output);
            exit(EXIT_FAILURE);
        }

        if (dup2(output_fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(output_fd);
            exit(EXIT_FAILURE);
        }

        close(output_fd);
    }
}

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
        apply_redirection(command);
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

int execute_pipeline(Pipeline *pipeline) {
    if (pipeline->command_count == 0) {
        return 0;
    }

    if (pipeline->command_count == 1) {
        return execute_external(&pipeline->commands[0]);
    }

    int pipefds[MAX_COMMANDS - 1][2];
    pid_t pids[MAX_COMMANDS];

    for (int i = 0; i < pipeline->command_count - 1; i++) {
        if (pipe(pipefds[i]) < 0) {
            perror("pipe");
            return 1;
        }
    }

    for (int i = 0; i < pipeline->command_count; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            if (i > 0) {
                if (dup2(pipefds[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            if (i < pipeline->command_count - 1) {
                if (dup2(pipefds[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            for (int j = 0; j < pipeline->command_count - 1; j++) {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }

            apply_redirection(&pipeline->commands[i]);

            execvp(pipeline->commands[i].argv[0], pipeline->commands[i].argv);
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        pids[i] = pid;
    }

    for (int i = 0; i < pipeline->command_count - 1; i++) {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }

    int status = 0;

    for (int i = 0; i < pipeline->command_count; i++) {
        if (waitpid(pids[i], &status, 0) < 0) {
            perror("waitpid");
            return 1;
        }
    }

    return status;
}
