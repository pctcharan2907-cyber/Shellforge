#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

void setup_background_handler(void);
int execute_external(Command *command);
int execute_pipeline(Pipeline *pipeline);

#endif
