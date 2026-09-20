#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    TOKEN_WORD,
    TOKEN_END
} TokenType;

typedef struct {
    TokenType type;
    char *value;
} Token;

Token create_token(TokenType type, const char *value);
void free_token(Token *token);
const char *token_type_to_string(TokenType type);

#endif
