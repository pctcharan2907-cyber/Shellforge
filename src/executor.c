#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "executor.h"

static void sigchld_handler(int sig) {
    int saved_errno = errno;
    (void)sig;

    while (waitpid(-1, NULL, WNOHANG) > 0) {
    }

    errno = saved_errno;
}

void setup_background_handler(void) {
    struct sigaction sa;

    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("sigaction");
    }
}

static void redirect_background_input(Command *command) {
    if (command->background && command->input == NULL) {
        int null_fd = open("/dev/null", O_RDONLY);
        if (null_fd < 0) {
            perror("/dev/null");
            exit(EXIT_FAILURE);
        }

        if (dup2(null_fd, STDIN_FILENO) < 0) {
            perror("dup2");
            close(null_fd);
            exit(EXIT_FAILURE);
        }

        close(null_fd);
    }
}

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
        redirect_background_input(command);
        apply_redirection(command);
        execvp(command->argv[0], command->argv);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    int status = 0;

    if (command->background) {
        printf("[Background PID: %d]\n", pid);
        return 0;
    }

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
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
    int background = pipeline->commands[pipeline->command_count - 1].background;

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
            if (background && i == 0 && pipeline->commands[i].input == NULL) {
                int null_fd = open("/dev/null", O_RDONLY);
                if (null_fd < 0) {
                    perror("/dev/null");
                    exit(EXIT_FAILURE);
                }

                if (dup2(null_fd, STDIN_FILENO) < 0) {
                    perror("dup2");
                    close(null_fd);
                    exit(EXIT_FAILURE);
                }

                close(null_fd);
            }

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

    if (background) {
        printf("[Background Pipeline PID: %d]\n", pids[0]);
        return 0;
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
