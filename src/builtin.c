#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtin.h"
#include "jobs.h"

void print_student_id(void)
{
    printf("Student ID: 2500032293\n");
}

static int builtin_cd(Command *command)
{
    char *directory = NULL;

    if (command->argc == 1) {
        directory = getenv("HOME");
    } else if (command->argc == 2) {
        directory = command->argv[1];
    } else {
        fprintf(stderr, "cd: too many arguments\n");
        return 1;
    }

    if (directory == NULL) {
        fprintf(stderr, "cd: HOME not set\n");
        return 1;
    }

    if (chdir(directory) != 0) {
        perror("cd");
        return 1;
    }

    return 0;
}

static int builtin_pwd(Command *command)
{
    char cwd[1024];

    if (command->argc > 1) {
        fprintf(stderr, "pwd: too many arguments\n");
        return 1;
    }

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return 1;
    }

    printf("%s\n", cwd);
    return 0;
}

static int builtin_echo(Command *command)
{
    for (int i = 1; i < command->argc; i++) {
        printf("%s", command->argv[i]);

        if (i < command->argc - 1) {
            printf(" ");
        }
    }

    printf("\n");
    return 0;
}

static int builtin_exit(Command *command)
{
    if (command->argc > 1) {
        fprintf(stderr, "exit: too many arguments\n");
        return 1;
    }

    return -1;
}

static int parse_job_id(Command *command, int *job_id)
{
    if (command->argc != 2) {
        fprintf(stderr, "%s: usage: %s %%job_id\n",
                command->argv[0], command->argv[0]);
        return -1;
    }

    const char *text = command->argv[1];
    if (text[0] == '%') {
        text++;
    }

    char *end = NULL;
    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value <= 0 || value > INT_MAX) {
        fprintf(stderr, "%s: invalid job ID: %s\n",
                command->argv[0], command->argv[1]);
        return -1;
    }

    *job_id = (int)value;
    return 0;
}

int builtin_jobs(Command *command)
{
    if (command->argc != 1) {
        fprintf(stderr, "jobs: too many arguments\n");
        return 1;
    }

    jobs_print();
    return 0;
}

static int wait_for_job(job_t *job)
{
    int status;
    int stopped = 0;
    pid_t result;

    for (;;) {
        result = waitpid(-job->pgid, &status, WUNTRACED);

        if (result > 0) {
            job_update_process(result, status);

            if (WIFSTOPPED(status)) {
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
            perror("fg: waitpid");
            break;
        }
    }

    if (isatty(STDIN_FILENO) &&
        tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        perror("fg: restore terminal");
    }

    if (stopped) {
        job_stop(job->pgid);
        printf("[%d] Stopped %s\n", job->job_id, job->command);
    } else {
        int finished_job_id = job->job_id;
        job_done(job->pgid);
        job_remove(finished_job_id);
    }

    return 0;
}

int builtin_fg(Command *command)
{
    int job_id;

    if (parse_job_id(command, &job_id) != 0) {
        return 1;
    }

    job_t *job = job_find(job_id);
    if (job == NULL || job->state == JOB_DONE) {
        fprintf(stderr, "fg: no such job: %d\n", job_id);
        return 1;
    }

    printf("%s\n", job->command);

    if (isatty(STDIN_FILENO) &&
        tcsetpgrp(STDIN_FILENO, job->pgid) == -1) {
        perror("fg: tcsetpgrp");
        return 1;
    }

    if (job->state == JOB_STOPPED) {
        if (kill(-job->pgid, SIGCONT) == -1) {
            perror("fg: SIGCONT");

            if (isatty(STDIN_FILENO)) {
                tcsetpgrp(STDIN_FILENO, getpgrp());
            }

            return 1;
        }

        job_continue(job->pgid);
    }

    return wait_for_job(job);
}

int builtin_bg(Command *command)
{
    int job_id;

    if (parse_job_id(command, &job_id) != 0) {
        return 1;
    }

    job_t *job = job_find(job_id);
    if (job == NULL || job->state == JOB_DONE) {
        fprintf(stderr, "bg: no such job: %d\n", job_id);
        return 1;
    }

    if (kill(-job->pgid, SIGCONT) == -1) {
        perror("bg: SIGCONT");
        return 1;
    }

    job_continue(job->pgid);
    printf("[%d] %s &\n", job->job_id, job->command);
    return 0;
}

int is_builtin(Command *command)
{
    if (command->argc == 0) {
        return 0;
    }

    return strcmp(command->argv[0], "cd") == 0 ||
           strcmp(command->argv[0], "pwd") == 0 ||
           strcmp(command->argv[0], "echo") == 0 ||
           strcmp(command->argv[0], "exit") == 0 ||
           strcmp(command->argv[0], "jobs") == 0 ||
           strcmp(command->argv[0], "fg") == 0 ||
           strcmp(command->argv[0], "bg") == 0;
}

int execute_builtin(Command *command)
{
    if (command->argc == 0) {
        return 0;
    }

    if (strcmp(command->argv[0], "cd") == 0) {
        return builtin_cd(command);
    }

    if (strcmp(command->argv[0], "pwd") == 0) {
        return builtin_pwd(command);
    }

    if (strcmp(command->argv[0], "echo") == 0) {
        return builtin_echo(command);
    }

    if (strcmp(command->argv[0], "exit") == 0) {
        return builtin_exit(command);
    }

    if (strcmp(command->argv[0], "jobs") == 0) {
        return builtin_jobs(command);
    }

    if (strcmp(command->argv[0], "fg") == 0) {
        return builtin_fg(command);
    }

    if (strcmp(command->argv[0], "bg") == 0) {
        return builtin_bg(command);
    }

    return 1;
}
