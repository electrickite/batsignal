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
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "battery.h"
#include "main.h"
#include "notify.h"
#include "options.h"

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
  int bat_index;
  BatteryState battery;
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
    .battery_names = NULL,
    .battery_count = 0,
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

  if (config.show_notifications)
    notification_init(config.appname, config.icon, config.notification_expires);
  set_message_command(config.msgcmd);

  if (config.battery_count > 0) {
    bat_index = validate_batteries(config.battery_names, config.battery_count);
    if (config.battery_required && bat_index >= 0)
      err(EXIT_FAILURE, "Battery %s not found", config.battery_names[bat_index]);
  } else {
    config.battery_count = find_batteries(&config.battery_names);
  }
  if (config.battery_count < 1)
    errx(EXIT_FAILURE, "No batteries found");

  printf("Using batteries:   %s", config.battery_names[0]);
  for (int i = 1; i < config.battery_count; i++)
    printf(", %s", config.battery_names[i]);
  printf("\n");

  if (config.daemonize && daemon(1, 1) < 0) {
    err(EXIT_FAILURE, "Failed to daemonize");
  }

  battery.names = config.battery_names;
  battery.count = config.battery_count;
  update_battery_state(&battery, config.battery_required);

  for(;;) {
    previous_discharging_status = battery.discharging;
    update_battery_state(&battery, config.battery_required);
    duration = config.multiplier;

    if (battery.discharging) { /* discharging */
      if (config.danger && battery.level <= config.danger) {
        if (battery.state != STATE_DANGER) {
          battery.state = STATE_DANGER;
          if (config.dangercmd[0] != '\0')
            if (system(config.dangercmd) == -1) { /* Ignore command errors... */ }
        }

      } else if (config.critical && battery.level <= config.critical) {
        if (battery.state != STATE_CRITICAL) {
          battery.state = STATE_CRITICAL;
          notify(config.criticalmsg, NOTIFY_URGENCY_CRITICAL, battery);
        }

      } else if (config.warning && battery.level <= config.warning) {
        if (!config.fixed)
          duration = (battery.level - config.critical) * config.multiplier;

        if (battery.state != STATE_WARNING) {
          battery.state = STATE_WARNING;
          notify(config.warningmsg, NOTIFY_URGENCY_NORMAL, battery);
        }

      } else {
        if (config.show_charging_msg && battery.discharging != previous_discharging_status) {
          notify(config.dischargingmsg, NOTIFY_URGENCY_NORMAL, battery);
        } else if (battery.state == STATE_FULL) {
          close_notification();
        }
        battery.state = STATE_DISCHARGING;
        if (!config.fixed)
          duration = (battery.level - config.warning) * config.multiplier;
      }

    } else { /* charging */
      if ((config.full && battery.state != STATE_FULL) && (battery.level >= config.full || battery.full)) {
        battery.state = STATE_FULL;
        notify(config.fullmsg, NOTIFY_URGENCY_NORMAL, battery);

      } else if (config.show_charging_msg && battery.discharging != previous_discharging_status) {
        battery.state = STATE_AC;
        notify(config.chargingmsg, NOTIFY_URGENCY_NORMAL, battery);

      } else {
        battery.state = STATE_AC;
        close_notification();
      }
    }

    if (config.multiplier == 0) {
      sigwaitinfo(&sigs, NULL);
    } else {
      timeout.tv_sec = duration;
      sigtimedwait(&sigs, NULL, &timeout);
    }

    if (config.run_once) break;
  }

  return EXIT_SUCCESS;
}
