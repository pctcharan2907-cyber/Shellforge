#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

int execute_external(Command *command);
int execute_pipeline(Pipeline *pipeline);

#endif
