/*
 * Copyright (c) 2018-2024 Corey Hinshaw
 */

#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Config {
  /* program operation options */
  bool daemonize;
  bool run_once;
  bool battery_required;
  bool show_notifications;
  bool show_charging_msg;
  bool help;
  bool version;

  /* Battery configuration */
  char **battery_names;
  int battery_count;

  /* check frequency multiplier (seconds) */
  int multiplier;
  bool fixed;

  /* battery warning levels */
  int warning;
  int critical;
  int danger;
  int full;

  /* messages for battery levels */
  char *warningmsg;
  char *criticalmsg;
  char *fullmsg;
  char *chargingmsg;
  char *dischargingmsg;

  /* run this system command if battery reaches danger level */
  char *dangercmd;

  /* run this system command to display a message */
  char *msgcmd;

  /* app name for notification */
  char *appname;

  /* specify the icon used in notifications */
  char *icon;

  /* specify when the notification should expire */
  int notification_expires;
} Config;

char* find_config_file();
char** read_config_file(char *path, int *argc, char *argv0);
void parse_args(int argc, char *argv[], Config *config);
void validate_options(Config *config);

#endif
