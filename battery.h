/*
 * Copyright (c) 2018-2024 Corey Hinshaw
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>

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

#define POWER_SUPPLY_ATTR_LENGTH 15

/* battery information */
typedef struct BatteryState {
  char **names;
  int count;
  bool discharging;
  bool full;
  char state;
  int level;
  int energy_full;
  int energy_now;
} BatteryState;

int find_batteries(char ***battery_names);
int validate_batteries(char **battery_names, int battery_count);
void update_battery_state(BatteryState *battery, bool required);

#endif
