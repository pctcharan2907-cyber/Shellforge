#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "builtin.h"

void print_student_id(void) {
    printf("Student ID: 2500032293\n");
}

static int builtin_cd(Command *command) {
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

static int builtin_pwd(Command *command) {
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

static int builtin_echo(Command *command) {
    for (int i = 1; i < command->argc; i++) {
        printf("%s", command->argv[i]);

        if (i < command->argc - 1) {
            printf(" ");
        }
    }

    printf("\n");
    return 0;
}

static int builtin_exit(Command *command) {
    if (command->argc > 1) {
        fprintf(stderr, "exit: too many arguments\n");
        return 1;
    }

    return -1;
}

int is_builtin(Command *command) {
    if (command->argc == 0) {
        return 0;
    }

    return strcmp(command->argv[0], "cd") == 0 ||
           strcmp(command->argv[0], "pwd") == 0 ||
           strcmp(command->argv[0], "echo") == 0 ||
           strcmp(command->argv[0], "exit") == 0;
}

int execute_builtin(Command *command) {
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

    return 1;
}
