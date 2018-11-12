/* See LICENSE file for copyright and license details. */

#include <err.h>
#include <libnotify/notify.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#undef strlcat
#undef strlcpy

#include "extern/arg.h"
#include "extern/strlcat.h"
#include "extern/strlcpy.h"
#include "extern/concat.h"

#include "config.h"

char *argv0;
char concat[];
static unsigned short int done;
static unsigned short int dflag;

int main(int argc, char *argv[])
{
	ARGBEGIN {
		case 'd':
			dflag = 1;
			break;
		case 'v':
			printf("batsignal %s\n", VERSION);
			return 0;
		default:
			fprintf(stderr, "usage: %s [-dhv]\n", argv0);
			return 1;
	} ARGEND

	if (dflag && daemon(1, 1) < 0) {
		err(1, "daemon");
	}

	while (!done) {
		char state[12];
		int perc;
		FILE *percfile;
		FILE *statefile;

		ccat(3, "/sys/class/power_supply/", battery, "/status");
		statefile = fopen(concat, "r");
		if (statefile == NULL) {
			warn("Failed to open file %s", concat);
			return 1;
		}
		fscanf(statefile, "%12s", state);
		fclose(statefile);

		if (strcmp(state, "Discharging") == 0) {
			ccat(3, "/sys/class/power_supply/", battery, "/capacity");
			percfile = fopen(concat, "r");
			if (percfile == NULL) {
				warn("Failed to open file %s", concat);
				return 1;
			}
			fscanf(percfile, "%i", &perc);
			fclose(percfile);

			if (perc <= danger) {
				system(dangercmd);
			} else if (perc <= critical && notify_init("batsignal")) {
				NotifyNotification *notification = notify_notification_new("Warning", "Battery is critically low.", NULL);
				notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
				notify_notification_show(notification, NULL);
				g_object_unref(notification);
				notify_uninit();
			} else if (perc <= warning && notify_init("batsignal")) {
				NotifyNotification *notification = notify_notification_new("Notice", "Battery is low.", NULL);
				notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
				notify_notification_show(notification, NULL);
				g_object_unref(notification);
				notify_uninit();
			}
		}

		sleep(repeat);
	}

	return 0;
}
