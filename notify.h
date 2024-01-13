/*
 * Copyright (c) 2018-2024 Corey Hinshaw
 */

#ifndef NOTIFY_H
#define NOTIFY_H

#include <libnotify/notification.h>
#include <libnotify/notify.h>
#include "battery.h"

void notification_init(char* appname, char *icon, int expires);
void set_message_command(char *command);
void notify(char *msg, NotifyUrgency urgency, BatteryState battery);
void close_notification();

#endif
