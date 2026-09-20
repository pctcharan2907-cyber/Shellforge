#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"

void init_lexer(Lexer *lexer, const char *input) {
    lexer->input = input;
    lexer->position = 0;
    lexer->length = strlen(input);
}

Token get_next_token(Lexer *lexer) {
    while (lexer->position < lexer->length &&
           isspace((unsigned char)lexer->input[lexer->position])) {
        lexer->position++;
    }

    if (lexer->position >= lexer->length) {
        return create_token(TOKEN_END, "END");
    }

    int start = lexer->position;

    while (lexer->position < lexer->length &&
           !isspace((unsigned char)lexer->input[lexer->position])) {
        lexer->position++;
    }

    int word_length = lexer->position - start;
    char *word = malloc(word_length + 1);
    if (word == NULL) {
        exit(EXIT_FAILURE);
    }

    strncpy(word, lexer->input + start, word_length);
    word[word_length] = '\0';

    Token token = create_token(TOKEN_WORD, word);
    free(word);

    return token;
}

Token *tokenize_input(const char *input, int *token_count) {
    Lexer lexer;
    init_lexer(&lexer, input);

    int capacity = 10;
    int count = 0;
    Token *tokens = malloc(sizeof(Token) * capacity);
    if (tokens == NULL) {
        exit(EXIT_FAILURE);
    }

    while (1) {
        if (count >= capacity) {
            capacity *= 2;
            Token *new_tokens = realloc(tokens, sizeof(Token) * capacity);
            if (new_tokens == NULL) {
                free_tokens(tokens, count);
                exit(EXIT_FAILURE);
            }
            tokens = new_tokens;
        }

        Token token = get_next_token(&lexer);
        tokens[count++] = token;

        if (token.type == TOKEN_END) {
            break;
        }
    }

    *token_count = count;
    return tokens;
}

void free_tokens(Token *tokens, int token_count) {
    if (tokens == NULL) {
        return;
    }

    for (int i = 0; i < token_count; i++) {
        free_token(&tokens[i]);
    }

    free(tokens);
}
