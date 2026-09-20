#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"
#include "executor.h"

#define MAX_INPUT_SIZE 1024
#define MAX_HISTORY 100

void print_tokens(Token *tokens, int token_count) {
    printf("\n============ TOKENS ============\n");

    for (int i = 0; i < token_count; i++) {
        printf("%d : %s", i, token_type_to_string(tokens[i].type));

        if (tokens[i].value != NULL) {
            printf("\t%s", tokens[i].value);
        }

        printf("\n");
    }

    printf("================================\n");
}

void print_history(char history[][MAX_INPUT_SIZE], int history_count) {
    printf("\n-------- Command History --------\n");

    for (int i = 0; i < history_count; i++) {
        printf("%d  %s\n", i + 1, history[i]);
    }

    printf("---------------------------------\n");
}

int main(void) {
    char input[MAX_INPUT_SIZE];
    char history[MAX_HISTORY][MAX_INPUT_SIZE];
    int history_count = 0;

    printf("ShellForge\n");
    printf("A Unix Style Shell written in C\n");

    while (1) {
        printf("\nshellforge$ ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) {
            continue;
        }

        if (history_count < MAX_HISTORY) {
            strcpy(history[history_count], input);
            history_count++;
        }

        char *expanded_input = expand_variables(input);

        int token_count = 0;
        Token *tokens = tokenize_input(expanded_input, &token_count);

        print_tokens(tokens, token_count);

        Pipeline pipeline = parse_pipeline(tokens, token_count);
        print_pipeline(&pipeline);

        if (pipeline.command_count == 1 && is_builtin(&pipeline.commands[0])) {
            int result = execute_builtin(&pipeline.commands[0]);

            free_pipeline(&pipeline);
            free_tokens(tokens, token_count);
            free(expanded_input);

            if (result == -1) {
                break;
            }

            continue;
        }

        if (strcmp(input, "history") == 0) {
            print_history(history, history_count);
        } else {
            execute_pipeline(&pipeline);
        }

        free_pipeline(&pipeline);
        free_tokens(tokens, token_count);
        free(expanded_input);
    }

    return 0;
}
