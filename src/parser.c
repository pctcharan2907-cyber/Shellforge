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

void init_pipeline(Pipeline *pipeline) {
    pipeline->command_count = 0;

    for (int i = 0; i < MAX_COMMANDS; i++) {
        init_command(&pipeline->commands[i]);
    }
}

Command parse_tokens(Token *tokens, int token_count) {
    Pipeline pipeline = parse_pipeline(tokens, token_count);

    Command command;
    init_command(&command);

    if (pipeline.command_count > 0) {
        command = pipeline.commands[0];
        init_command(&pipeline.commands[0]);
    }

    free_pipeline(&pipeline);
    return command;
}

Pipeline parse_pipeline(Token *tokens, int token_count) {
    Pipeline pipeline;
    init_pipeline(&pipeline);

    int current = 0;
    pipeline.command_count = 1;

    for (int i = 0; i < token_count; i++) {
        if (tokens[i].type == TOKEN_END) {
            break;
        }

        char *value = tokens[i].value;

        if (strcmp(value, "|") == 0) {
            if (pipeline.command_count < MAX_COMMANDS) {
                current++;
                pipeline.command_count++;
            }
        } else if (strcmp(value, "<") == 0 && i + 1 < token_count) {
            pipeline.commands[current].input = copy_string(tokens[++i].value);
        } else if (strcmp(value, ">") == 0 && i + 1 < token_count) {
            pipeline.commands[current].output = copy_string(tokens[++i].value);
            pipeline.commands[current].append = 0;
        } else if (strcmp(value, ">>") == 0 && i + 1 < token_count) {
            pipeline.commands[current].output = copy_string(tokens[++i].value);
            pipeline.commands[current].append = 1;
        } else if (strcmp(value, "&") == 0) {
            pipeline.commands[current].background = 1;
        } else {
            if (pipeline.commands[current].argc < MAX_ARGS - 1) {
                pipeline.commands[current].argv[pipeline.commands[current].argc] = copy_string(value);
                pipeline.commands[current].argc++;
            }
        }
    }

    for (int i = 0; i < pipeline.command_count; i++) {
        pipeline.commands[i].argv[pipeline.commands[i].argc] = NULL;
    }

    return pipeline;
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

void print_pipeline(Pipeline *pipeline) {
    printf("\n========== PIPELINE ==========\n");

    for (int i = 0; i < pipeline->command_count; i++) {
        Command *command = &pipeline->commands[i];

        printf("Command %d\n", i + 1);
        printf("------------------------------\n");
        printf("Arguments\n");

        for (int j = 0; j < command->argc; j++) {
            printf("argv[%d] = %s\n", j, command->argv[j]);
        }

        printf("Input      : %s\n", command->input ? command->input : "None");
        printf("Output     : %s\n", command->output ? command->output : "None");
        printf("Append     : %s\n", command->append ? "Yes" : "No");
        printf("Background : %s\n", command->background ? "Yes" : "No");
    }

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
    command->argc = 0;
}

void free_pipeline(Pipeline *pipeline) {
    for (int i = 0; i < pipeline->command_count; i++) {
        free_command(&pipeline->commands[i]);
    }

    pipeline->command_count = 0;
}
