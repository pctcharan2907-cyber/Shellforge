#ifndef BUILTIN_H
#define BUILTIN_H

#include "parser.h"

void print_student_id(void);
int is_builtin(Command *command);
int execute_builtin(Command *command);

int builtin_jobs(Command *cmd);
int builtin_fg(Command *cmd);
int builtin_bg(Command *cmd);

#endif
