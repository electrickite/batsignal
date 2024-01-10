#define _DEFAULT_SOURCE
#include "options.h"
#include <err.h>
#include <errno.h>
#include <libnotify/notify.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "defs.h"

static int split(char *in, char delim, char ***out)
{
  int count = 1;

  char *p = in;
  while (*p != '\0') {
    if (*p == delim)
      count++;
    p++;
  }

  *out = (char **)realloc(*out, sizeof(char **) * (count));
  if (*out == NULL)
    err(EXIT_FAILURE, "Memory allocation failed");

  (*out)[0] = strtok(in, &delim);
  for (int i = 1; i < count; i++) {
    char *tok = strtok(NULL, &delim);
    if (tok)
      (*out)[i] = tok;
    else {
      count--;
      i--;
    }
  }

  return count;
}

char* find_config_file()
{
  char *config_file = NULL;
  char *config_paths[5];
  char *home = getenv("HOME");
  char *config_home = getenv("XDG_CONFIG_HOME");

  config_paths[0] = getenv(PROGUPPER "_CONFIG");
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

void parse_args(int argc, char *argv[], Config *config)
{
  signed int c;
  optind = 1;

  while ((c = getopt(argc, argv, ":hvboiew:c:d:f:pW:C:D:F:P:U:M:Nn:m:a:I:")) != -1) {
    switch (c) {
      case 'h':
        config->help = true;
        break;
      case 'v':
        config->version = true;
        break;
      case 'b':
        config->daemonize = true;
        break;
      case 'o':
        config->run_once = true;
        break;
      case 'i':
        config->battery_required = false;
        break;
      case 'w':
        config->warning = strtoul(optarg, NULL, 10);
        break;
      case 'c':
        config->critical = strtoul(optarg, NULL, 10);
        break;
      case 'd':
        config->danger = strtoul(optarg, NULL, 10);
        break;
      case 'f':
        config->full = strtoul(optarg, NULL, 10);
        config->fixed = true;
        break;
      case 'p':
        config->show_charging_msg = 1;
        config->fixed = true;
        break;
      case 'W':
        config->warningmsg = optarg;
        break;
      case 'C':
        config->criticalmsg = optarg;
        break;
      case 'D':
        config->dangercmd = optarg;
        break;
      case 'F':
        config->fullmsg = optarg;
        break;
      case 'P':
        config->chargingmsg = optarg;
        break;
      case 'U':
        config->dischargingmsg = optarg;
        break;
      case 'M':
        config->msgcmd = optarg;
        break;
      case 'N':
        config->show_notifications = false;
        break;
      case 'n':
        config->battery_name_specified = true;
        config->amount_batteries = split(optarg, ',', &config->battery_names);
        break;
      case 'm':
        if (optarg[0] == '+') {
          config->fixed = true;
          config->multiplier = strtoul(optarg + 1, NULL, 10);
        } else {
          config->multiplier = strtoul(optarg, NULL, 10);
        }
        break;
      case 'a':
        config->appname = optarg;
        break;
      case 'I':
        config->icon = optarg;
        break;
      case 'e':
        config->notification_expires = NOTIFY_EXPIRES_DEFAULT;
        break;
      case '?':
        errx(EXIT_FAILURE, "Unknown option `-%c'.", optopt);
      case ':':
        errx(EXIT_FAILURE, "Option -%c requires an argument.", optopt);
    }
  }
}

void validate_options(Config *config)
{
  int lowlvl = config->danger;
  char *rangemsg = "Option -%c must be between 0 and %i.";

  /* Sanity check numberic values */
  if (config->warning > 100 || config->warning < 0) errx(EXIT_FAILURE, rangemsg, 'w', 100);
  if (config->critical > 100 || config->critical < 0) errx(EXIT_FAILURE, rangemsg, 'c', 100);
  if (config->danger > 100 || config->danger < 0) errx(EXIT_FAILURE, rangemsg, 'd', 100);
  if (config->full > 100 || config->full < 0) errx(EXIT_FAILURE, rangemsg, 'f', 100);
  if (config->multiplier < 0 || config->multiplier > 3600) errx(EXIT_FAILURE, rangemsg, 'm', 3600);

  /* Enssure levels are correctly ordered */
  if (config->warning && config->warning <= config->critical)
    errx(EXIT_FAILURE, "Warning level must be greater than critical.");
  if (config->critical && config->critical <= config->danger)
    errx(EXIT_FAILURE, "Critical level must be greater than danger.");

  /* Find highest warning level */
  if (config->warning || config->critical)
    lowlvl = config->warning ? config->warning : config->critical;

  /* Ensure the full level is higher than the warning levels */
  if (config->full && config->full <= lowlvl)
    errx(EXIT_FAILURE, "Option -f must be greater than %i.", lowlvl);
}
