#ifndef PARSER_H
#define PARSER_H

#include "token.h"

#define MAX_ARGS 64

typedef struct {
    char *argv[MAX_ARGS];
    int argc;
    char *input;
    char *output;
    int append;
    int background;
} Command;

void init_command(Command *command);
Command parse_tokens(Token *tokens, int token_count);
void print_command(Command *command);
void free_command(Command *command);

#endif

