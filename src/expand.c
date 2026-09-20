#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "expand.h"

char *expand_variables(const char *input) {
    int capacity = 1024;
    char *result = malloc(capacity);
    if (result == NULL) {
        exit(EXIT_FAILURE);
    }

    int result_pos = 0;

    for (int i = 0; input[i] != '\0'; i++) {
        if (input[i] == '$') {
            i++;

            char variable_name[128];
            int name_pos = 0;

            while (input[i] != '\0' &&
                   (isalnum((unsigned char)input[i]) || input[i] == '_')) {
                if (name_pos < 127) {
                    variable_name[name_pos++] = input[i];
                }
                i++;
            }

            variable_name[name_pos] = '\0';
            i--;

            char *value = getenv(variable_name);
            if (value != NULL) {
                int value_len = strlen(value);

                while (result_pos + value_len + 1 >= capacity) {
                    capacity *= 2;
                    char *new_result = realloc(result, capacity);
                    if (new_result == NULL) {
                        free(result);
                        exit(EXIT_FAILURE);
                    }
                    result = new_result;
                }

                strcpy(result + result_pos, value);
                result_pos += value_len;
            }
        } else {
            if (result_pos + 2 >= capacity) {
                capacity *= 2;
                char *new_result = realloc(result, capacity);
                if (new_result == NULL) {
                    free(result);
                    exit(EXIT_FAILURE);
                }
                result = new_result;
            }

            result[result_pos++] = input[i];
        }
    }

    result[result_pos] = '\0';
    return result;
}
