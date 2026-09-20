#ifndef BUILTIN_H
#define BUILTIN_H

#include "parser.h"

int is_builtin(Command *command);
int execute_builtin(Command *command);

#endif
