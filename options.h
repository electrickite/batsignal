#ifndef OPTIONS_H
#define OPTIONS_H

#include <stddef.h>

char* find_config_file();
char** read_config_file(char *path, int *argc, char *argv0);

#endif
