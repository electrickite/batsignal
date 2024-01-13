/*
 * Copyright (c) 2018-2024 Corey Hinshaw
 */

#define _DEFAULT_SOURCE
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include "battery.h"
#include "notify.h"

static NotifyNotification *notification = NULL;
static char *notification_icon = NULL;

static char *msgcmd = NULL;
static char *msgcmdbuf = NULL;

void notification_init(char* appname, char *icon, int expires)
{
  notification_icon = icon;
  if (!notify_init(appname))
    err(EXIT_FAILURE, "Failed to initialize notifications");
  notification = notify_notification_new("", NULL, icon);
  notify_notification_set_timeout(notification, expires);
}

void set_message_command(char *command)
{
  msgcmd = command;
}

void notify(char *msg, NotifyUrgency urgency, BatteryState battery)
{
  char body[20];
  char level[8];
  size_t needed;

  if (msgcmd[0] != '\0') {
    snprintf(level, 8, "%d", battery.level);
    needed = snprintf(NULL, 0, msgcmd, msg, level);
    msgcmdbuf = realloc(msgcmdbuf, needed + 1);
    if (msgcmdbuf == NULL)
      err(EXIT_FAILURE, "Memory allocation failed");
    sprintf(msgcmdbuf, msgcmd, msg, level);
    if (system(msgcmdbuf) == -1) { /* Ignore command errors... */ }
  }

  if (notification && msg[0] != '\0') {
    sprintf(body, "Battery level: %u%%", battery.level);
    notify_notification_update(notification, msg, body, notification_icon);
    notify_notification_set_urgency(notification, urgency);
    notify_notification_show(notification, NULL);
  }
}

void close_notification()
{
  notify_notification_close(notification, NULL);
}
