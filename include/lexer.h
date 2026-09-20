#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct {
    const char *input;
    int position;
    int length;
} Lexer;

void init_lexer(Lexer *lexer, const char *input);
Token get_next_token(Lexer *lexer);
Token *tokenize_input(const char *input, int *token_count);
void free_tokens(Token *tokens, int token_count);

#endif

