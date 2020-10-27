/*
 * Copyright 2018-2020 Corey Hinshaw
 * Copyright 2016-2017 Aaron Marcher
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <libnotify/notify.h>
#include "version.h"

#define PROGNAME "batsignal"

/* battery states */
#define STATE_AC 0
#define STATE_DISCHARGING 1
#define STATE_WARNING 2
#define STATE_CRITICAL 3
#define STATE_DANGER 4
#define STATE_FULL 5

/* system paths */
#define POWER_SUPPLY_SUBSYSTEM "/sys/class/power_supply"

/* program operation options */
static unsigned char daemonize = 0;
static unsigned char battery_required = 1;
static unsigned char battery_name_specified = 0;

/* battery information */
static char *battery_name = "BAT0";
static unsigned char battery_discharging = 0;
static unsigned char battery_state = STATE_AC;
static unsigned int battery_level = 100;
static char *attr_path;

/* check frequency multiplier (seconds) */
static unsigned int multiplier = 60;

/* battery warning levels */
static unsigned int warning = 15;
static unsigned int critical = 5;
static unsigned int danger = 2;
static unsigned int full = 0;

/* messages for battery levels */
static char *warningmsg = "Battery is low";
static char *criticalmsg = "Battery is critically low";
static char *fullmsg = "Battery is full";

/* run this system command if battery reaches danger level */
static char *dangercmd = "";

/* app name for notification */
static char *appname = PROGNAME;

void print_version()
{
  printf("%s %s\n", PROGNAME, VERSION);
}

void print_help()
{
  printf("Usage: %s [OPTIONS]\n\
\n\
Sends battery level notifications.\n\
\n\
Options:\n\
    -h             print this help message\n\
    -v             print program version information\n\
    -b             run as background daemon\n\
    -i             ignore missing battery errors\n\
    -w LEVEL       battery warning LEVEL\n\
                   (default: 15)\n\
    -c LEVEL       critical battery LEVEL\n\
                   (default: 5)\n\
    -d LEVEL       battery danger LEVEL\n\
                   (default: 2)\n\
    -f LEVEL       full battery LEVEL\n\
                   (default: disabled)\n\
    -W MESSAGE     show MESSAGE when battery is at warning level\n\
    -C MESSAGE     show MESSAGE when battery is at critical level\n\
    -D COMMAND     run COMMAND when battery is at danger level\n\
    -F MESSAGE     show MESSAGE when battery is full\n\
    -n NAME        use battery NAME\n\
                   (default: BAT0)\n\
    -m SECONDS     minimum number of SECONDS to wait between battery checks\n\
                   (default: 60)\n\
    -a NAME        app NAME used in desktop notifications\n\
                   (default: %s)\n\
", PROGNAME, PROGNAME);
}

void notify(char *msg, NotifyUrgency urgency)
{
  char body[20];
  sprintf(body, "Battery level: %u%%", battery_level);

  if (msg[0] != '\0' && notify_init(appname)) {
    NotifyNotification *notification = notify_notification_new(msg, body, NULL);
    notify_notification_set_urgency(notification, urgency);
    notify_notification_set_timeout(notification, NOTIFY_EXPIRES_NEVER);
    notify_notification_show(notification, NULL);
    g_object_unref(notification);
    notify_uninit();
  }
}

void update_battery()
{
  char state[12];
  FILE *file;

  sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/status", battery_name);
  file = fopen(attr_path, "r");
  if (file == NULL || fscanf(file, "%12s", state) == 0) {
    if (battery_required)
      err(EXIT_FAILURE, "Could not read %s", attr_path);
    battery_discharging = 0;
    goto cleanup;
  }
  fclose(file);

  battery_discharging = strcmp(state, "Discharging") == 0;

  sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/capacity", battery_name);
  file = fopen(attr_path, "r");
  if (file == NULL || fscanf(file, "%u", &battery_level) == 0) {
    if (battery_required)
      err(EXIT_FAILURE, "Could not read %s", attr_path);
  }

cleanup:
  if (file) {
    fclose(file);
  }
}

void parse_args(int argc, char *argv[])
{
  signed int c;

  while ((c = getopt(argc, argv, ":hvbiw:c:d:f:W:C:D:F:n:m:a:")) != -1) {
    switch (c) {
      case 'h':
        print_help();
        exit(0);
      case 'v':
        print_version();
        exit(0);
      case 'b':
        daemonize = 1;
        break;
      case 'i':
        battery_required = 0;
        break;
      case 'w':
        warning = atoi(optarg);
        break;
      case 'c':
        critical = atoi(optarg);
        break;
      case 'd':
        danger = atoi(optarg);
        break;
      case 'f':
        full = atoi(optarg);
        break;
      case 'W':
        warningmsg = optarg;
        break;
      case 'C':
        criticalmsg = optarg;
        break;
      case 'D':
        dangercmd = optarg;
        break;
      case 'F':
        fullmsg = optarg;
        break;
      case 'n':
        battery_name_specified = 1;
        battery_name = optarg;
        break;
      case 'm':
        multiplier = atoi(optarg);
        break;
      case 'a':
        appname = optarg;
        break;
      case '?':
        errx(EXIT_FAILURE, "Unknown option `-%c'.", optopt);
      case ':':
        errx(EXIT_FAILURE, "Option -%c requires an argument.", optopt);
    }
  }
}

void validate_options()
{
  unsigned int lowlvl = danger;
  char *rangemsg = "Option -%c must be between 0 and %i.";

  /* Sanity check numberic values */
  if (warning > 100) errx(EXIT_FAILURE, rangemsg, 'w', 100);
  if (critical > 100) errx(EXIT_FAILURE, rangemsg, 'c', 100);
  if (danger > 100) errx(EXIT_FAILURE, rangemsg, 'd', 100);
  if (full > 100) errx(EXIT_FAILURE, rangemsg, 'f', 100);
  if (multiplier <= 0) errx(EXIT_FAILURE, "Option -m must be greater than 0");

  /* Enssure levels are correctly ordered */
  if (warning && warning <= critical)
    errx(EXIT_FAILURE, "Warning level must be greater than critical.");
  if (critical && critical <= danger)
    errx(EXIT_FAILURE, "Critical level must be greater than danger.");

  /* Find highest warning level */
  if (warning || critical)
    lowlvl = warning ? warning : critical;

  /* Ensure the full level is higher than the warning levels */
  if (full && full <= lowlvl)
    errx(EXIT_FAILURE, "Option -f must be greater than %i.", lowlvl);
}

unsigned char is_battery(char *name)
{
  FILE *file;
  char type[10] = "";

  sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/type", name);
  file = fopen(attr_path, "r");
  if (file != NULL) {
    if (fscanf(file, "%10s", type) == 0) { /* Continue... */ }
    fclose(file);
  }
  return strcmp(type, "Battery") == 0;
}

void find_battery()
{
  unsigned int path_len = strlen(POWER_SUPPLY_SUBSYSTEM) + 15;
  unsigned int name_len = strlen(battery_name);
  unsigned int entry_name_len = 0;
  DIR *dir;
  struct dirent *entry;

  attr_path = malloc(path_len + name_len);
  if (attr_path == NULL)
    err(EXIT_FAILURE, "Memory allocation failed");

  if (is_battery(battery_name)) {
    return;
  } else if (battery_name_specified) {
    goto nobat;
  }

  dir = opendir(POWER_SUPPLY_SUBSYSTEM);
  if (dir) {
    while ((entry = readdir(dir)) != NULL) {
      entry_name_len = strlen(entry->d_name);
      if (entry_name_len > name_len) {
        name_len = entry_name_len;
        attr_path = realloc(attr_path, path_len + name_len);
        if (attr_path == NULL)
          err(EXIT_FAILURE, "Memory allocation failed");
      }
      if (is_battery(entry->d_name)) {
        battery_name = strdup(entry->d_name);
        if (battery_name == NULL)
          err(EXIT_FAILURE, "Memory allocation failed");
        closedir(dir);
        return;
      }
    }
    closedir(dir);
  }

nobat:
  if (battery_required)
    err(EXIT_FAILURE, "Battery %s not found", battery_name);
}

int main(int argc, char *argv[])
{
  unsigned int duration;

  parse_args(argc, argv);
  validate_options();
  find_battery();
  printf("Using battery: %s\n", battery_name);

  if (daemonize && daemon(1, 1) < 0) {
    err(EXIT_FAILURE, "Failed to daemonize");
  }

  for(;;) {
    update_battery();
    duration = multiplier;

    if (battery_discharging) { /* discharging */
      if (danger && battery_level <= danger && battery_state != STATE_DANGER) {
        battery_state = STATE_DANGER;
        if (dangercmd[0] != '\0')
          if (system(dangercmd) == -1) { /* Ignore command errors... */ }

      } else if (critical && battery_level <= critical && battery_state != STATE_CRITICAL) {
        battery_state = STATE_CRITICAL;
        notify(criticalmsg, NOTIFY_URGENCY_CRITICAL);

      } else if (warning && battery_level <= warning) {
        duration = (battery_level - critical) * multiplier;
        if (battery_state != STATE_WARNING) {
          battery_state = STATE_WARNING;
          notify(warningmsg, NOTIFY_URGENCY_NORMAL);
        }

      } else {
        battery_state = STATE_DISCHARGING;
        duration = (battery_level - warning) * multiplier;
      }

    } else { /* charging */
        battery_state = STATE_AC;
        if (full && battery_level >= full && battery_state != STATE_FULL) {
            battery_state = STATE_FULL;
            notify(fullmsg, NOTIFY_URGENCY_NORMAL);
        }
    }

    sleep(duration);
  }

  return EXIT_SUCCESS;
}
