/* See LICENSE file for copyright and license details. */

/* battery to check */
static const char battery[] = "BAT0";

/* how often to check */
static const unsigned int repeat = 60;

/* order should be danger < critical < warning; numbers in percent; check is <= */
static const signed int warning = 10;
static const signed int critical = 5;
static const signed int danger = 2;

/* what command to run if battery is under "danger" value */
static const char dangercmd[] = "sudo zzz";
