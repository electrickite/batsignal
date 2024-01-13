/*
 * Copyright (c) 2018-2024 Corey Hinshaw
 */

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "battery.h"

static char *attr_path = NULL;

static void set_attributes(char *battery_name, char **now_attribute, char **full_attribute)
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

static bool is_type_battery(char *name)
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

static bool has_capacity_field(char *name)
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

static bool is_battery(char *name)
{
  return is_type_battery(name) && has_capacity_field(name);
}

int find_batteries(char ***battery_names)
{
  unsigned int path_len = strlen(POWER_SUPPLY_SUBSYSTEM) + POWER_SUPPLY_ATTR_LENGTH;
  unsigned int entry_name_len = 5;
  int battery_count = 0;
  DIR *dir;
  struct dirent *entry;

  attr_path = realloc(attr_path, path_len + entry_name_len);

  dir = opendir(POWER_SUPPLY_SUBSYSTEM);
  if (dir) {
    while ((entry = readdir(dir)) != NULL) {
      if (strlen(entry->d_name) > entry_name_len) {
        entry_name_len = strlen(entry->d_name);
        attr_path = realloc(attr_path, path_len + entry_name_len);
        if (attr_path == NULL)
          err(EXIT_FAILURE, "Memory allocation failed");
      }

      if (is_battery(entry->d_name)) {
        *battery_names = realloc(*battery_names, sizeof(char *) * (battery_count+1));
        if (*battery_names == NULL)
          err(EXIT_FAILURE, "Memory allocation failed");
        (*battery_names)[battery_count] = strdup(entry->d_name);
        if ((*battery_names)[battery_count] == NULL)
          err(EXIT_FAILURE, "Memory allocation failed");
        battery_count++;
      }
    }
    closedir(dir);
  }

  return battery_count;
}

int validate_batteries(char **battery_names, int battery_count)
{
  unsigned int path_len = strlen(POWER_SUPPLY_SUBSYSTEM) + POWER_SUPPLY_ATTR_LENGTH;
  unsigned int name_len = 5;
  int return_value = -1;

  attr_path = realloc(attr_path, path_len + name_len);

  for (int i = 0; i < battery_count; i++) {
    if (strlen(battery_names[i]) > name_len) {
      name_len = strlen(battery_names[i]);
      attr_path = realloc(attr_path, path_len + name_len);
      if (attr_path == NULL)
        err(EXIT_FAILURE, "Memory allocation failed");
    }
    if (!is_battery(battery_names[i]) && return_value < 0) {
      return_value = i;
    }
  }
  return return_value;
}

void update_battery_state(BatteryState *battery, bool required)
{
  char state[15];
  char *now_attribute;
  char *full_attribute;
  unsigned int tmp_now;
  unsigned int tmp_full;
  FILE *file;

  battery->discharging = false;
  battery->full = true;
  battery->energy_now = 0;
  battery->energy_full = 0;
  set_attributes(battery->names[0], &now_attribute, &full_attribute);

  /* iterate through all batteries */
  for (int i = 0; i < battery->count; i++) {
    sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/status", battery->names[i]);
    file = fopen(attr_path, "r");
    if (file == NULL || fscanf(file, "%12s", state) == 0) {
      if (required)
        err(EXIT_FAILURE, "Could not read %s", attr_path);
      battery->discharging |= 0;
      if (file)
        fclose(file);
      continue;
    }
    fclose(file);

    battery->discharging |= strcmp(state, POWER_SUPPLY_DISCHARGING) == 0;
    battery->full &= strcmp(state, POWER_SUPPLY_FULL) == 0;

    sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/%s", battery->names[i], now_attribute);
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
      sprintf(attr_path, POWER_SUPPLY_SUBSYSTEM "/%s/%s", battery->names[i], full_attribute);
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

    battery->energy_now += tmp_now;
    battery->energy_full += tmp_full;
  }

  battery->level = round(100.0 * battery->energy_now / battery->energy_full);
}
