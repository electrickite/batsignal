/*
 * Copyright (c) 2018-2022 Corey Hinshaw
 * Copyright (c) 2016-2017 Aaron Marcher
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
#include <libnotify/notify.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

/* Battery state strings */
#define POWER_SUPPLY_FULL "Full"
#define POWER_SUPPLY_DISCHARGING "Discharging"

/* program operation options */
static char daemonize = 0;
static char run_once = 0;
static char battery_required = 1;
static char show_notifications = 1;
static char battery_name_specified = 0;

/* battery information */
static char **battery_names;
static int amount_batteries = 1;
static char battery_discharging = 0;
static char battery_full = 1;
static char battery_state = STATE_AC;
static int battery_level = 100;
static int energy_full = 0;
static int energy_now = 0;
static char *attr_path;

/* check frequency multiplier (seconds) */
static int multiplier = 60;

/* battery warning levels */
static int warning = 15;
static int critical = 5;
static int danger = 2;
static int full = 0;

/* messages for battery levels */
static char *warningmsg = "Battery is low";
static char *criticalmsg = "Battery is critically low";
static char *fullmsg = "Battery is full";

/* run this system command if battery reaches danger level */
static char *dangercmd = "";

/* run this system command to display a message */
static char *msgcmd = "";
static char *msgcmdbuf;

/* app name for notification */
static char *appname = PROGNAME;

/* specify the icon used in notifications */
static char *icon = NULL;

/* specify when the notification should expire */
static int notification_expires = NOTIFY_EXPIRES_NEVER;

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
    -o             check battery once and exit\n\
    -i             ignore missing battery errors\n\
    -e             cause notifications to expire\n\
    -N             disable desktop notifications\n\
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
    -M COMMAND     send each message using COMMAND\n\
    -n NAME        use battery NAME - multiple batteries separated by commas\n\
                   (default: BAT0)\n\
    -m SECONDS     minimum number of SECONDS to wait between battery checks\n\
                   0 SECONDS disables polling and waits for USR1 signal\n\
                   (default: 60)\n\
    -a NAME        app NAME used in desktop notifications\n\
                   (default: %s)\n\
    -I ICON        display specified ICON in notifications\n\
", PROGNAME, PROGNAME);
}

void notify(char *msg, NotifyUrgency urgency)
{
  char body[20];
  char level[8];
  size_t needed;

  if (msgcmd[0] != '\0') {
    snprintf(level, 8, "%d", battery_level);
    needed = snprintf(NULL, 0, msgcmd, msg, level);
    msgcmdbuf = realloc(msgcmdbuf, needed + 1);
    if (msgcmdbuf == NULL)
      err(EXIT_FAILURE, "Memory allocation failed");
    sprintf(msgcmdbuf, msgcmd, msg, level);
    if (system(msgcmdbuf) == -1) { /* Ignore command errors... */ }
  }

  if (show_notifications && msg[0] != '\0' && notify_init(appname)) {
    sprintf(body, "Battery level: %u%%", battery_level);

    NotifyNotification *notification = notify_notification_new(msg, body, icon);
    notify_notification_set_urgency(notification, urgency);
    notify_notification_set_timeout(notification, notification_expires);
    notify_notification_show(notification, NULL);
    g_object_unref(notification);
    notify_uninit();
  }
}

void set_attributes(char **now_attribute, char **full_attribute)
{
  char *battery_name = battery_names[0];
  sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/charge_now", battery_name);
  if (access(attr_path, F_OK) == 0) {
    *now_attribute = "charge_now";
    *full_attribute = "charge_full";
  } else {
    sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/energy_now", battery_name);
    if (access(attr_path, F_OK) == 0) {
      *now_attribute = "energy_now";
      *full_attribute = "energy_full";
    } else {
      *now_attribute = "capacity";
      *full_attribute = NULL;
    }
  }
}

void update_batteries()
{
  char state[15];
  char *battery_name;
  char *now_attribute;
  char *full_attribute;
  unsigned int tmp_now;
  unsigned int tmp_full;
  FILE *file;

  battery_discharging = 0;
  battery_full = 1;
  energy_now = 0;
  energy_full = 0;
  set_attributes(&now_attribute, &full_attribute);

  /* iterate through all batteries */
  for (int i = 0; i < amount_batteries; i++) {
    battery_name = battery_names[i];

    sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/status", battery_name);
    file = fopen(attr_path, "r");
    if (file == NULL || fscanf(file, "%12s", state) == 0) {
      if (battery_required)
        err(EXIT_FAILURE, "Could not read %s", attr_path);
      battery_discharging |= 0;
      if (file)
        fclose(file);
      continue;
    }
    fclose(file);

    battery_discharging |= strcmp(state, POWER_SUPPLY_DISCHARGING) == 0;
    battery_full &= strcmp(state, POWER_SUPPLY_FULL) == 0;

    sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/%s", battery_name, now_attribute);
    file = fopen(attr_path, "r");
    if (file == NULL || fscanf(file, "%u", &tmp_now) == 0) {
      if (battery_required)
        err(EXIT_FAILURE, "Could not read %s", attr_path);
      if (file)
        fclose(file);
      continue;
    }
    fclose(file);

    if (full_attribute != NULL) {
      sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/%s", battery_name, full_attribute);
      file = fopen(attr_path, "r");
      if (file == NULL || fscanf(file, "%u", &tmp_full) == 0) {
        if (battery_required)
          err(EXIT_FAILURE, "Could not read %s", attr_path);
        if (file)
          fclose(file);
        continue;
      }
      fclose(file);
    } else {
      tmp_full = 100;
    }

    energy_now += tmp_now;
    energy_full += tmp_full;
  }

  battery_level = round(100.0 * energy_now / energy_full);
}

int split(char *in, char delim, char ***out)
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

void parse_args(int argc, char *argv[])
{
  signed int c;

  while ((c = getopt(argc, argv, "-:hvboiew:c:d:f:W:C:D:F:M:Nn:m:a:I:")) != -1) {
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
      case 'o':
        run_once = 1;
        break;
      case 'i':
        battery_required = 0;
        break;
      case 'w':
        warning = strtoul(optarg, NULL, 10);
        break;
      case 'c':
        critical = strtoul(optarg, NULL, 10);
        break;
      case 'd':
        danger = strtoul(optarg, NULL, 10);
        break;
      case 'f':
        full = strtoul(optarg, NULL, 10);
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
      case 'M':
        msgcmd = optarg;
        break;
      case 'N':
        show_notifications = 0;
        break;
      case 'n':
        battery_name_specified = 1;
        amount_batteries = split(optarg, ',', &battery_names);
        break;
      case 'm':
        multiplier = strtoul(optarg, NULL, 10);
        break;
      case 'a':
        appname = optarg;
        break;
      case 'I':
        icon = optarg;
        break;
      case 'e':
        notification_expires = NOTIFY_EXPIRES_DEFAULT;
        break;
      case '?':
        errx(EXIT_FAILURE, "Unknown option `-%c'.", optopt);
      case ':':
        errx(EXIT_FAILURE, "Option -%c requires an argument.", optopt);
      case 1:
        errx(EXIT_FAILURE, "Unknown argument %s.", optarg);
    }
  }
}

void validate_options()
{
  int lowlvl = danger;
  char *rangemsg = "Option -%c must be between 0 and %i.";

  /* Sanity check numberic values */
  if (warning > 100 || warning < 0) errx(EXIT_FAILURE, rangemsg, 'w', 100);
  if (critical > 100 || critical < 0) errx(EXIT_FAILURE, rangemsg, 'c', 100);
  if (danger > 100 || danger < 0) errx(EXIT_FAILURE, rangemsg, 'd', 100);
  if (full > 100 || full < 0) errx(EXIT_FAILURE, rangemsg, 'f', 100);
  if (multiplier < 0 || multiplier > 3600) errx(EXIT_FAILURE, rangemsg, 'm', 3600);

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

void find_batteries()
{
  unsigned int path_len = strlen(POWER_SUPPLY_SUBSYSTEM) + 15;
  unsigned int entry_name_len = 0;
  unsigned int name_len = 0;
  DIR *dir;
  struct dirent *entry;

  if (battery_name_specified) {
    for (int i = 0; i < amount_batteries; i++) {
      if (strlen(battery_names[i]) > name_len) {
        name_len = strlen(battery_names[i]);
        attr_path = realloc(attr_path, path_len + name_len);
      }
      if (is_battery(battery_names[i])) {
        continue;
      } else if (battery_name_specified && battery_required) {
        err(EXIT_FAILURE, "Battery %s not found", battery_names[i]);
      }
    }
  } else {
    dir = opendir(POWER_SUPPLY_SUBSYSTEM);
    if (dir) {
      int i = 0;
      while ((entry = readdir(dir)) != NULL) {
        if (strlen(entry->d_name) > entry_name_len) {
          entry_name_len = strlen(entry->d_name);
          attr_path = realloc(attr_path, path_len + entry_name_len);
        }
        if (attr_path == NULL)
          err(EXIT_FAILURE, "Memory allocation failed");

        if (is_battery(entry->d_name)) {
          battery_names = (char **)realloc(battery_names, sizeof(char *) * i);
          battery_names[i] = strdup(entry->d_name);
          if (battery_names[i] == NULL)
            err(EXIT_FAILURE, "Memory allocation failed");
          i++;
        }
      }
      amount_batteries = i;
    }

    closedir(dir);
  }
}

void cleanup()
{
  if (notify_is_initted()) {
    notify_uninit();
  }
}

void signal_handler()
{
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
  unsigned int duration;
  sigset_t sigs;
  struct timespec timeout = { .tv_sec = 0 };

  sigemptyset(&sigs);
  sigaddset(&sigs, SIGUSR1);
  atexit(cleanup);
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  sigprocmask(SIG_BLOCK, &sigs, NULL);

  parse_args(argc, argv);
  validate_options();
  find_batteries();

  printf("Using batteries: %s", battery_names[0]);
  for (int i = 1; i < amount_batteries; i++)
    printf(", %s", battery_names[i]);
  printf("\n");

  if (daemonize && daemon(1, 1) < 0) {
    err(EXIT_FAILURE, "Failed to daemonize");
  }

  for(;;) {
    update_batteries();
    duration = multiplier;

    if (battery_discharging) { /* discharging */
      if (danger && battery_level <= danger) {
        if (battery_state != STATE_DANGER) {
          battery_state = STATE_DANGER;
          if (dangercmd[0] != '\0')
            if (system(dangercmd) == -1) { /* Ignore command errors... */ }
        }

      } else if (critical && battery_level <= critical) {
        if (battery_state != STATE_CRITICAL) {
          battery_state = STATE_CRITICAL;
          notify(criticalmsg, NOTIFY_URGENCY_CRITICAL);
        }

      } else if (warning && battery_level <= warning) {
        if (!full)
          duration = (battery_level - critical) * multiplier;

        if (battery_state != STATE_WARNING) {
          battery_state = STATE_WARNING;
          notify(warningmsg, NOTIFY_URGENCY_NORMAL);
        }

      } else {
        battery_state = STATE_DISCHARGING;
        if (!full)
          duration = (battery_level - warning) * multiplier;
      }

    } else { /* charging */
      if (battery_state != STATE_FULL)
        battery_state = STATE_AC;

      if (full && battery_state != STATE_FULL) {
        if (battery_level >= full || battery_full) {
          battery_state = STATE_FULL;
          notify(fullmsg, NOTIFY_URGENCY_NORMAL);
        }
      }
    }

    if (run_once) {
      break;
    } else if (multiplier == 0) {
      sigwaitinfo(&sigs, NULL);
    } else {
      timeout.tv_sec = duration;
      sigtimedwait(&sigs, NULL, &timeout);
    }
  }

  return EXIT_SUCCESS;
}
