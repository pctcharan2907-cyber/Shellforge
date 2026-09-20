#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"

Token create_token(TokenType type, const char *value) {
    Token token;
    token.type = type;

    if (value != NULL) {
        token.value = malloc(strlen(value) + 1);
        if (token.value == NULL) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        strcpy(token.value, value);
    } else {
        token.value = NULL;
    }

    return token;
}

void free_token(Token *token) {
    if (token != NULL && token->value != NULL) {
        free(token->value);
        token->value = NULL;
    }
}

const char *token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_WORD:
            return "WORD";
        case TOKEN_END:
            return "END";
        default:
            return "UNKNOWN";
    }
}
