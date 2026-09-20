#ifndef PARSER_H
#define PARSER_H

#include "token.h"

#define MAX_ARGS 64
#define MAX_COMMANDS 16

typedef struct {
    char *argv[MAX_ARGS];
    int argc;
    char *input;
    char *output;
    int append;
    int background;
} Command;

typedef struct {
    Command commands[MAX_COMMANDS];
    int command_count;
} Pipeline;

void init_command(Command *command);
void init_pipeline(Pipeline *pipeline);

Command parse_tokens(Token *tokens, int token_count);
Pipeline parse_pipeline(Token *tokens, int token_count);

void print_command(Command *command);
void print_pipeline(Pipeline *pipeline);

void free_command(Command *command);
void free_pipeline(Pipeline *pipeline);

#endif
