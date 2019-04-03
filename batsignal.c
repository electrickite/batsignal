/* See LICENSE file for copyright and license details. */

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <libnotify/notify.h>

#define PROGNAME "batsignal"

/* battery states */
#define STATE_AC 0
#define STATE_DISCHARGING 1
#define STATE_WARNING 2
#define STATE_CRITICAL 3
#define STATE_DANGER 4
#define STATE_FULL 5

/* program operation options */
static char daemonize = 0;
static char battery_required = 1;

/* battery information */
static char *battery_name = "BAT0";
static char battery_discharging = 0;
static char battery_state = STATE_AC;
static unsigned int battery_level = 100;

/* check frequency multiplier (seconds) */
static unsigned int multiplier = 60;

/* battery warning levels */
static unsigned int warning = 15;
static unsigned int critical = 5;
static unsigned int danger = 2;
static unsigned int full = 95;

/* messages for warning and critical levels */
static char *warningmsg = "Battery is low";
static char *criticalmsg = "Battery is critically low";

/* message for full battery */
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
Display a battery level notifications.\n\
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
                   (default: 95)\n\
    -W MESSAGE     show MESSAGE when battery is at warning level\n\
    -C MESSAGE     show MESSAGE when battery is at critical level\n\
    -D COMMAND     run COMMAND when battery is at danger level\n\
    -F MESSAGE     show MESSAGE when battery is full\n\
    -n NAME        battery device NAME\n\
                   (default: BAT0)\n\
    -m SECONDS     minimum number of SECONDS to wait between battery checks\n\
                   (default: 60)\n\
    -a APP_NAME    specify app name for the notification\n\
                   (default: %s)\n\
", PROGNAME, PROGNAME);
}

void notify(char *msg)
{
  char body[20];
  sprintf(body, "Battery level: %u%%", battery_level);

  if (msg[0] != '\0' && notify_init(appname)) {
    NotifyNotification *notification = notify_notification_new(msg, body, NULL);
    notify_notification_set_urgency(notification, NOTIFY_URGENCY_CRITICAL);
    notify_notification_set_timeout(notification, NOTIFY_EXPIRES_NEVER);
    notify_notification_show(notification, NULL);
    g_object_unref(notification);
    notify_uninit();
  }
}

void update_battery()
{
  char state[12];
  char *path = malloc(strlen(battery_name) + 35);
  FILE *file;

  sprintf(path, "/sys/class/power_supply/%s/status", battery_name);
  file = fopen(path, "r");
  if (file == NULL) {
    if (battery_required)
      err(3, "Could not open %s", path);
    battery_discharging = 0;
    return;
  }
  fscanf(file, "%12s", state);
  fclose(file);

  battery_discharging = strcmp(state, "Discharging") == 0;

  sprintf(path, "/sys/class/power_supply/%s/capacity", battery_name);
  file = fopen(path, "r");
  if (file == NULL) {
    if (battery_required)
      err(3, "Could not open %s", path);
    return;
  }
  fscanf(file, "%u", &battery_level);
  fclose(file);

  free(path);
}

void parse_args(int argc, char *argv[])
{
  int c;

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
        battery_name = optarg;
        break;
      case 'm':
        multiplier = atoi(optarg);
        break;
      case 'a':
        appname = optarg;
        break;
      case '?':
        errx(1, "Unknown option `-%c'.", optopt);
      case ':':
        errx(1, "Option -%c requires an argument.", optopt);
    }
  }
}

void validate_options()
{
  char *rangemsg = "Option -%c must be between 0 and %i.";
  char argerr = 1;

  if (warning > 100) errx(argerr, rangemsg, 'w', 100);
  if (critical > 100) errx(argerr, rangemsg, 'c', 100);
  if (danger > 100) errx(argerr, rangemsg, 'd', 100);
  if (full > 100) errx(argerr, rangemsg, 'f', 100);
  if (multiplier == 0) errx(argerr, "Option -m must be greater than 0");

  if (warning && warning <= critical)
    errx(argerr, "Warning level must be greater than critical.");
  if (critical && critical <= danger)
    errx(argerr, "Critical level must be greater than danger.");
}

int main(int argc, char *argv[])
{
  int duration;

  parse_args(argc, argv);
  validate_options();

  if (daemonize && daemon(1, 1) < 0) {
    err(2, "daemon");
  }

  for(;;) {
    update_battery();
    duration = multiplier;

    if (battery_discharging) {
      if (danger && battery_level <= danger && battery_state != STATE_DANGER) {
        battery_state = STATE_DANGER;
        if (dangercmd[0] != '\0') system(dangercmd);

      } else if (critical && battery_level <= critical && battery_state != STATE_CRITICAL) {
        battery_state = STATE_CRITICAL;
        notify(criticalmsg);

      } else if (warning && battery_level <= warning) {
        duration = (battery_level - critical) * multiplier;
        if (battery_state != STATE_WARNING) {
          battery_state = STATE_WARNING;
          notify(warningmsg);
        }

      } else {
        battery_state = STATE_DISCHARGING;
        duration = (battery_level - warning) * multiplier;
      }

    } else { /* charging */
        battery_state = STATE_AC;
        if (full && battery_level >= full && battery_state != STATE_FULL) {
            battery_state = STATE_FULL;
            notify(fullmsg);
	}
    }

    sleep(duration);
  }

  return 0;
}
