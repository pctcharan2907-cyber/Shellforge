#ifndef BUILTIN_H
#define BUILTIN_H

#include "parser.h"

void print_student_id(void);
int is_builtin(Command *command);
int execute_builtin(Command *command);

#endif
