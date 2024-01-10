#define _DEFAULT_SOURCE
#include "options.h"
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "defs.h"

char* find_config_file()
{
  char *config_file = NULL;
  char *config_paths[5];
  char *home = getenv("HOME");
  char *config_home = getenv("XDG_CONFIG_HOME");

  config_paths[0] = getenv(PROGNAME_UPPER "_CONFIG");
  if (config_home == NULL || config_home[0] == '\0') {
    config_paths[1] = malloc(strlen(home) + strlen("/.config/" PROGNAME) + 1);
    if (config_paths[1] == NULL)
      err(EXIT_FAILURE, "Memory allocation failed");
    strcpy(config_paths[1], home);
    strcat(config_paths[1], "/.config/" PROGNAME);
  } else {
    config_paths[1] = malloc(strlen(config_home) + strlen("/" PROGNAME) + 1);
    if (config_paths[1] == NULL)
      err(EXIT_FAILURE, "Memory allocation failed");
    strcpy(config_paths[1], config_home);
    strcat(config_paths[1], "/" PROGNAME);
  }
  config_paths[2] = malloc(strlen(home) + strlen("/." PROGNAME) + 1);
  if (config_paths[2] == NULL)
    err(EXIT_FAILURE, "Memory allocation failed");
  strcpy(config_paths[2], home);
  strcat(config_paths[2], "/." PROGNAME);
  config_paths[3] = "/usr/local/etc/" PROGNAME;
  config_paths[4] = "/etc/" PROGNAME;

  for (int i = 0; i < 5; i++) {
	  if (config_paths[i] && access(config_paths[i], F_OK) == 0) {
      config_file = strdup(config_paths[i]);
      if (config_file == NULL)
        err(EXIT_FAILURE, "Memory allocation failed");
    }
  }

  free(config_paths[1]);
  free(config_paths[2]);
  return config_file;
}

char** read_config_file(char *path, int *argc, char *argv0)
{
  char **argv;
  FILE *file;
  size_t numlines_allocated = 128;
  char **ptr;
  char *end;
  ssize_t numchars;
  size_t maxbytes = 0;
  size_t buffer_increment = 512 * sizeof(char*);

  file = fopen(path, "r");
  if (file == NULL) {
    err(EXIT_FAILURE, "Could not read %s", path);
  }

  argv = malloc(numlines_allocated);
  argv[0] = argv0;
  *argc = 1;
  ptr = argv;
  ptr++;

  while((numchars = getline(ptr, &maxbytes, file)) != -1) {
    maxbytes = 0;
    end = *ptr + strlen(*ptr) - 1;
    while(end >= *ptr && ((unsigned char)*end == '\n' || (unsigned char)*end == '\r'))
      end--;
    end[1] = '\0';
    if (*ptr[0] == '\0' || *ptr[0] == '#')
      continue;

    ptr++; (*argc)++;
    if((size_t)*argc > (numlines_allocated/sizeof(char *))) {
      argv = realloc(argv, numlines_allocated + buffer_increment);
      if (argv == NULL)
        err(EXIT_FAILURE, "Memory allocation failed");
      numlines_allocated += buffer_increment;
      ptr = argv + *argc;
    }
  }

  *(ptr) = 0;
  argv = realloc(argv, sizeof(char*) * *argc+1);
  if (argv == NULL)
    err(EXIT_FAILURE, "Memory allocation failed");

  if (file)
    fclose(file);
  return argv;
}
