#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

static char *copy_string(const char *text) {
    char *copy = malloc(strlen(text) + 1);
    if (copy == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    strcpy(copy, text);
    return copy;
}

void init_command(Command *command) {
    command->argc = 0;
    command->input = NULL;
    command->output = NULL;
    command->append = 0;
    command->background = 0;

    for (int i = 0; i < MAX_ARGS; i++) {
        command->argv[i] = NULL;
    }
}

Command parse_tokens(Token *tokens, int token_count) {
    Command command;
    init_command(&command);

    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == TOKEN_END) {
            break;
        }

        char *value = tokens[i].value;

        if (strcmp(value, "<") == 0 && i + 1 < token_count) {
            command.input = copy_string(tokens[++i].value);
        } else if (strcmp(value, ">") == 0 && i + 1 < token_count) {
            command.output = copy_string(tokens[++i].value);
            command.append = 0;
        } else if (strcmp(value, ">>") == 0 && i + 1 < token_count) {
            command.output = copy_string(tokens[++i].value);
            command.append = 1;
        } else if (strcmp(value, "&") == 0) {
            command.background = 1;
        } else {
            if (command.argc < MAX_ARGS - 1) {
                command.argv[command.argc] = copy_string(value);
                command.argc++;
            }
        }
    }

    command.argv[command.argc] = NULL;
    return command;
}

void print_command(Command *command) {
    printf("\n========== PIPELINE ==========\n");
    printf("Command 1\n");
    printf("------------------------------\n");
    printf("Arguments\n");

    for (int i = 0; i < command->argc; i++) {
        printf("argv[%d] = %s\n", i, command->argv[i]);
    }

    printf("Input      : %s\n", command->input ? command->input : "None");
    printf("Output     : %s\n", command->output ? command->output : "None");
    printf("Append     : %s\n", command->append ? "Yes" : "No");
    printf("Background : %s\n", command->background ? "Yes" : "No");
    printf("==============================\n");
}

void free_command(Command *command) {
    for (int i = 0; i < command->argc; i++) {
        free(command->argv[i]);
        command->argv[i] = NULL;
    }

    free(command->input);
    free(command->output);

    command->input = NULL;
    command->output = NULL;
}
