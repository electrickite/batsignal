/*
 * Copyright (c) 2018-2024 Corey Hinshaw
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

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <libnotify/notification.h>
#include <libnotify/notify.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "options.h"
#include "defs.h"

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

/* battery information */
static bool battery_discharging = false;
static bool battery_full = true;
static char battery_state = STATE_AC;
static int battery_level = 100;
static int energy_full = 0;
static int energy_now = 0;
static char *attr_path;

/* Shared notification instance */
NotifyNotification *notification = NULL;

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
    -p             show message when battery begins charging/discharging\n\
    -W MESSAGE     show MESSAGE when battery is at warning level\n\
    -C MESSAGE     show MESSAGE when battery is at critical level\n\
    -D COMMAND     run COMMAND when battery is at danger level\n\
    -F MESSAGE     show MESSAGE when battery is full\n\
    -P MESSAGE     battery charging MESSAGE\n\
    -U MESSAGE     battery discharging MESSAGE\n\
    -M COMMAND     send each message using COMMAND\n\
    -n NAME        use battery NAME - multiple batteries separated by commas\n\
                   (default: BAT0)\n\
    -m SECONDS     minimum number of SECONDS to wait between battery checks\n\
                   0 SECONDS disables polling and waits for USR1 signal\n\
                   Prefixing with a + will always check at SECONDS interval\n\
                   (default: 60)\n\
    -a NAME        app NAME used in desktop notifications\n\
                   (default: %s)\n\
    -I ICON        display specified ICON in notifications\n\
", PROGNAME, PROGNAME);
}

void update_notification(char *msg, NotifyUrgency urgency, Config *config)
{
  char body[20];
  char level[8];
  size_t needed;
  static char *msgcmdbuf = NULL;

  if (config->msgcmd[0] != '\0') {
    snprintf(level, 8, "%d", battery_level);
    needed = snprintf(NULL, 0, config->msgcmd, msg, level);
    msgcmdbuf = realloc(msgcmdbuf, needed + 1);
    if (msgcmdbuf == NULL)
      err(EXIT_FAILURE, "Memory allocation failed");
    sprintf(msgcmdbuf, config->msgcmd, msg, level);
    if (system(msgcmdbuf) == -1) { /* Ignore command errors... */ }
  }

  if (notification && config->show_notifications && msg[0] != '\0') {
    sprintf(body, "Battery level: %u%%", battery_level);

    notify_notification_update(notification, msg, body, config->icon);
    notify_notification_set_urgency(notification, urgency);
    notify_notification_set_timeout(notification, config->notification_expires);
    notify_notification_show(notification, NULL);
  }
}

void set_attributes(char *battery_name, char **now_attribute, char **full_attribute)
{
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

void update_batteries(char **battery_names, int amount_batteries, bool required)
{
  char state[15];
  char *battery_name;
  char *now_attribute;
  char *full_attribute;
  unsigned int tmp_now;
  unsigned int tmp_full;
  FILE *file;

  battery_discharging = false;
  battery_full = true;
  energy_now = 0;
  energy_full = 0;
  set_attributes(battery_names[0], &now_attribute, &full_attribute);

  /* iterate through all batteries */
  for (int i = 0; i < amount_batteries; i++) {
    battery_name = battery_names[i];

    sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/status", battery_name);
    file = fopen(attr_path, "r");
    if (file == NULL || fscanf(file, "%12s", state) == 0) {
      if (required)
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
      if (required)
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
        if (required)
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

bool is_type_battery(char *name)
{
  FILE *file;
  char type[11] = "";

  sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/type", name);
  file = fopen(attr_path, "r");
  if (file != NULL) {
    if (fscanf(file, "%10s", type) == 0) { /* Continue... */ }
    fclose(file);
  }
  return strcmp(type, "Battery") == 0;
}

bool has_capacity_field(char *name)
{
  FILE *file;
  int capacity = -1;
  char *now_attribute;
  char *full_attribute;

  set_attributes(name, &now_attribute, &full_attribute);

  if (strcmp(now_attribute, "capacity") == 0) {
    sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/capacity", name);
    file = fopen(attr_path, "r");
    if (file != NULL) {
      if (fscanf(file, "%d", &capacity) == 0) { /* Continue... */ }
      fclose(file);
    }
  } else {
    capacity = 1;
  }
  return capacity >= 0;
}

bool is_battery(char *name)
{
  return is_type_battery(name) && has_capacity_field(name);
}

void find_batteries(Config *config)
{
  unsigned int path_len = strlen(POWER_SUPPLY_SUBSYSTEM) + 15;
  unsigned int entry_name_len = 0;
  unsigned int name_len = 0;
  DIR *dir;
  struct dirent *entry;

  if (config->battery_name_specified) {
    for (int i = 0; i < config->amount_batteries; i++) {
      if (strlen(config->battery_names[i]) > name_len) {
        name_len = strlen(config->battery_names[i]);
        attr_path = realloc(attr_path, path_len + name_len);
      }
      if (is_battery(config->battery_names[i])) {
        continue;
      } else if (config->battery_name_specified && config->battery_required) {
        err(EXIT_FAILURE, "Battery %s not found", config->battery_names[i]);
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
          config->battery_names = (char **)realloc(config->battery_names, sizeof(char *) * i);
          config->battery_names[i] = strdup(entry->d_name);
          if (config->battery_names[i] == NULL)
            err(EXIT_FAILURE, "Memory allocation failed");
          i++;
        }
      }
      config->amount_batteries = i;
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
  bool previous_discharging_status;
  sigset_t sigs;
  struct timespec timeout = { .tv_sec = 0 };
  char *config_file = NULL;
  int conf_argc = 0;
  char **conf_argv;
  Config config = {
    .daemonize = false,
    .run_once = false,
    .battery_required = true,
    .show_notifications = true,
    .show_charging_msg = false,
    .help = false,
    .version = false,
    .battery_name_specified = false,
    .battery_names = NULL,
    .amount_batteries = 1,
    .multiplier = 60,
    .fixed = false,
    .warning = 15,
    .critical = 5,
    .danger = 2,
    .full = 0,
    .warningmsg = "Battery is low",
    .criticalmsg = "Battery is critically low",
    .fullmsg = "Battery is full",
    .chargingmsg = "Battery is charging",
    .dischargingmsg = "Battery is discharging",
    .dangercmd = "",
    .msgcmd = "",
    .appname = PROGNAME,
    .icon = NULL,
    .notification_expires = NOTIFY_EXPIRES_NEVER
  };

  sigemptyset(&sigs);
  sigaddset(&sigs, SIGUSR1);
  atexit(cleanup);
  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  sigprocmask(SIG_BLOCK, &sigs, NULL);

  config_file = find_config_file();
  if (config_file) {
    conf_argv = read_config_file(config_file, &conf_argc, NULL);
    parse_args(conf_argc, conf_argv, &config);
  }
  parse_args(argc, argv, &config);

  if (config.help) {
    print_help();
    return EXIT_SUCCESS;
  } else if (config.version) {
    print_version();
    return EXIT_SUCCESS;
  }

  validate_options(&config);
  if (config_file)
    printf("Using config file: %s\n", config_file);

  if (config.show_notifications) {
    if (!notify_init(config.appname))
      err(EXIT_FAILURE, "Failed to initialize notifications");
    notification = notify_notification_new(config.warningmsg, NULL, config.icon);
  }

  find_batteries(&config);
  printf("Using batteries:   %s", config.battery_names[0]);
  for (int i = 1; i < config.amount_batteries; i++)
    printf(", %s", config.battery_names[i]);
  printf("\n");

  if (config.daemonize && daemon(1, 1) < 0) {
    err(EXIT_FAILURE, "Failed to daemonize");
  }

  update_batteries(config.battery_names, config.amount_batteries, config.battery_required);

  for(;;) {
    previous_discharging_status = battery_discharging;
    update_batteries(config.battery_names, config.amount_batteries, config.battery_required);
    duration = config.multiplier;

    if (battery_discharging) { /* discharging */
      if (config.danger && battery_level <= config.danger) {
        if (battery_state != STATE_DANGER) {
          battery_state = STATE_DANGER;
          if (config.dangercmd[0] != '\0')
            if (system(config.dangercmd) == -1) { /* Ignore command errors... */ }
        }

      } else if (config.critical && battery_level <= config.critical) {
        if (battery_state != STATE_CRITICAL) {
          battery_state = STATE_CRITICAL;
          update_notification(config.criticalmsg, NOTIFY_URGENCY_CRITICAL, &config);
        }

      } else if (config.warning && battery_level <= config.warning) {
        if (!config.fixed)
          duration = (battery_level - config.critical) * config.multiplier;

        if (battery_state != STATE_WARNING) {
          battery_state = STATE_WARNING;
          update_notification(config.warningmsg, NOTIFY_URGENCY_NORMAL, &config);
        }

      } else {
        if (config.show_charging_msg && battery_discharging != previous_discharging_status) {
          update_notification(config.dischargingmsg, NOTIFY_URGENCY_NORMAL, &config);
        } else if (battery_state == STATE_FULL) {
          notify_notification_close(notification, NULL);
        }
        battery_state = STATE_DISCHARGING;
        if (!config.fixed)
          duration = (battery_level - config.warning) * config.multiplier;
      }

    } else { /* charging */
      if ((config.full && battery_state != STATE_FULL) && (battery_level >= config.full || battery_full)) {
        battery_state = STATE_FULL;
        update_notification(config.fullmsg, NOTIFY_URGENCY_NORMAL, &config);

      } else if (config.show_charging_msg && battery_discharging != previous_discharging_status) {
        battery_state = STATE_AC;
        update_notification(config.chargingmsg, NOTIFY_URGENCY_NORMAL, &config);

      } else {
        battery_state = STATE_AC;
        notify_notification_close(notification, NULL);
      }
    }

    if (config.run_once) {
      break;
    } else if (config.multiplier == 0) {
      sigwaitinfo(&sigs, NULL);
    } else {
      timeout.tv_sec = duration;
      sigtimedwait(&sigs, NULL, &timeout);
    }
  }

  return EXIT_SUCCESS;
}
