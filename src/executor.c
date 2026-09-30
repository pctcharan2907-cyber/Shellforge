#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "executor.h"
#include "jobs.h"

static pid_t shell_pgid;

static void sigchld_handler(int signal_number)
{
    (void)signal_number;
}

static void set_signal_action(int signal_number, void (*handler)(int))
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;

    if (sigaction(signal_number, &action, NULL) == -1) {
        perror("sigaction");
    }
}

void setup_background_handler(void)
{
    shell_pgid = getpgrp();

    set_signal_action(SIGCHLD, sigchld_handler);
    set_signal_action(SIGINT, SIG_IGN);
    set_signal_action(SIGQUIT, SIG_IGN);
    set_signal_action(SIGTSTP, SIG_IGN);
    set_signal_action(SIGTTIN, SIG_IGN);
    set_signal_action(SIGTTOU, SIG_IGN);

    if (isatty(STDIN_FILENO) &&
        tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        perror("tcsetpgrp");
    }
}

static void reset_child_signals(void)
{
    int signals[] = {
        SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD
    };
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_DFL;
    sigemptyset(&action.sa_mask);

    for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
        sigaction(signals[i], &action, NULL);
    }
}

static void apply_redirection(Command *command)
{
    if (command->input != NULL) {
        int input_fd = open(command->input, O_RDONLY);
        if (input_fd < 0) {
            perror(command->input);
            _exit(EXIT_FAILURE);
        }

        if (dup2(input_fd, STDIN_FILENO) < 0) {
            perror("dup2");
            close(input_fd);
            _exit(EXIT_FAILURE);
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
            _exit(EXIT_FAILURE);
        }

        if (dup2(output_fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(output_fd);
            _exit(EXIT_FAILURE);
        }

        close(output_fd);
    }
}

static void append_text(char *buffer, size_t size, const char *text)
{
    size_t used = strlen(buffer);

    if (used < size - 1) {
        snprintf(buffer + used, size - used, "%s", text);
    }
}

static void build_command_text(
    Command *commands,
    int command_count,
    int background,
    char *buffer,
    size_t size)
{
    buffer[0] = '\0';

    for (int i = 0; i < command_count; i++) {
        if (i > 0) {
            append_text(buffer, size, " |");
        }

        for (int j = 0; j < commands[i].argc; j++) {
            if (buffer[0] != '\0' &&
                buffer[strlen(buffer) - 1] != '|') {
                append_text(buffer, size, " ");
            }

            append_text(buffer, size, commands[i].argv[j]);
        }
    }

    if (background) {
        append_text(buffer, size, " &");
    }
}

static void close_pipes(int pipefds[][2], int pipe_count)
{
    for (int i = 0; i < pipe_count; i++) {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }
}

static int wait_for_foreground_job(int job_id, pid_t pgid, int *last_status)
{
    int status = 0;
    int stopped = 0;
    pid_t result;

    if (isatty(STDIN_FILENO) &&
        tcsetpgrp(STDIN_FILENO, pgid) == -1) {
        perror("tcsetpgrp");
    }

    for (;;) {
        result = waitpid(-pgid, &status, WUNTRACED | WCONTINUED);

        if (result > 0) {
            job_update_process(result, status);
            *last_status = status;

            job_t *job = job_find(job_id);
            if (WIFSTOPPED(status) &&
                job != NULL &&
                job->state == JOB_STOPPED) {
                stopped = 1;
                break;
            }

            continue;
        }

        if (result == -1 && errno == EINTR) {
            continue;
        }

        if (result == -1 && errno == ECHILD) {
            break;
        }

        if (result == -1) {
            perror("waitpid");
            break;
        }
    }

    if (isatty(STDIN_FILENO) &&
        tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
        perror("tcsetpgrp");
    }

    job_t *job = job_find(job_id);

    if (stopped && job != NULL) {
        printf("[%d] Stopped %s\n", job->job_id, job->command);
    } else if (job != NULL) {
        job_remove(job_id);
    }

    return stopped ? 0 : *last_status;
}

static int launch_commands(
    Command *commands,
    int command_count,
    int background,
    const char *command_text)
{
    int pipefds[MAX_COMMANDS - 1][2];
    pid_t pids[MAX_COMMANDS];
    int pipe_count = command_count - 1;
    pid_t pgid = 0;

    for (int i = 0; i < pipe_count; i++) {
        if (pipe(pipefds[i]) == -1) {
            perror("pipe");
            close_pipes(pipefds, i);
            return 1;
        }
    }

    for (int i = 0; i < command_count; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            close_pipes(pipefds, pipe_count);

            if (pgid > 0) {
                kill(-pgid, SIGTERM);
            }

            for (int j = 0; j < i; j++) {
                waitpid(pids[j], NULL, 0);
            }

            return 1;
        }

        if (pid == 0) {
            pid_t child_pgid = (i == 0) ? 0 : pgid;

            if (setpgid(0, child_pgid) == -1) {
                perror("setpgid");
                _exit(EXIT_FAILURE);
            }

            reset_child_signals();

            if (background && i == 0 && commands[i].input == NULL) {
                int null_fd = open("/dev/null", O_RDONLY);
                if (null_fd == -1 || dup2(null_fd, STDIN_FILENO) == -1) {
                    perror("/dev/null");
                    _exit(EXIT_FAILURE);
                }
                close(null_fd);
            }

            if (i > 0 && dup2(pipefds[i - 1][0], STDIN_FILENO) == -1) {
                perror("dup2");
                _exit(EXIT_FAILURE);
            }

            if (i < command_count - 1 &&
                dup2(pipefds[i][1], STDOUT_FILENO) == -1) {
                perror("dup2");
                _exit(EXIT_FAILURE);
            }

            close_pipes(pipefds, pipe_count);
            apply_redirection(&commands[i]);

            execvp(commands[i].argv[0], commands[i].argv);
            perror("execvp");
            _exit(EXIT_FAILURE);
        }

        if (i == 0) {
            pgid = pid;
        }

        if (setpgid(pid, pgid) == -1 &&
            errno != EACCES && errno != ESRCH) {
            perror("setpgid");
        }

        pids[i] = pid;
    }

    close_pipes(pipefds, pipe_count);

    int job_id = job_add(pgid, command_text, JOB_RUNNING);

    if (job_id >= 0) {
        for (int i = 0; i < command_count; i++) {
            if (job_track_process(job_id, pids[i]) == -1) {
                fprintf(stderr, "shellforge: could not track process\n");
            }
        }
    }

    if (background) {
        if (job_id >= 0) {
            printf("[%d] %d\n", job_id, (int)pgid);
        } else {
            printf("[Background process group: %d]\n", (int)pgid);
        }
        return 0;
    }

    if (job_id < 0) {
        int status = 0;

        if (isatty(STDIN_FILENO)) {
            tcsetpgrp(STDIN_FILENO, pgid);
        }

        for (int i = 0; i < command_count; i++) {
            if (waitpid(pids[i], &status, WUNTRACED) == -1) {
                perror("waitpid");
            }

            if (WIFSTOPPED(status)) {
                fprintf(stderr,
                        "shellforge: job table full; stopped job is untracked\n");
                break;
            }
        }

        if (isatty(STDIN_FILENO)) {
            tcsetpgrp(STDIN_FILENO, shell_pgid);
        }

        return status;
    }

    int status = 0;
    return wait_for_foreground_job(job_id, pgid, &status);
}

int execute_external(Command *command)
{
    if (command->argc == 0) {
        return 0;
    }

    char command_text[MAX_JOB_COMMAND];
    build_command_text(command, 1, command->background,
                       command_text, sizeof(command_text));

    return launch_commands(command, 1, command->background, command_text);
}

int execute_pipeline(Pipeline *pipeline)
{
    if (pipeline->command_count == 0) {
        return 0;
    }

    int background =
        pipeline->commands[pipeline->command_count - 1].background;
    char command_text[MAX_JOB_COMMAND];

    build_command_text(pipeline->commands, pipeline->command_count,
                       background, command_text, sizeof(command_text));

    return launch_commands(pipeline->commands, pipeline->command_count,
                           background, command_text);
}
