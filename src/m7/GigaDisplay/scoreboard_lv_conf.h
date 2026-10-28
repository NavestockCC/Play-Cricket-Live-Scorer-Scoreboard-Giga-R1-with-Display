#ifndef SCOREBOARD_LV_CONF_H
#define SCOREBOARD_LV_CONF_H

/*
 * Project LVGL configuration, selected via -DLV_CONF_PATH in platformio.ini.
 *
 * Starts from the LVGL 9 config the Giga core ships with Arduino_H7_Video
 * (tuned for the Giga Display Shield: 16-bit colour, 64KB LVGL heap, ...)
 * and overrides only what this project needs.
 *
 * Why not a file named lv_conf.h: Arduino_H7_Video ships its own lv_conf.h
 * earlier on the include path, so a project lv_conf.h is silently ignored.
 * LV_CONF_PATH is used by LVGL itself, Arduino_H7_Video and this project
 * alike, so every part of the build sees the same configuration.
 */
#include "lv_conf_9.h"

/* Fonts (LVGL names these LV_FONT_MONTSERRAT_<size>) */
#undef  LV_FONT_MONTSERRAT_16
#define LV_FONT_MONTSERRAT_16 1  /* score card headings */
#undef  LV_FONT_MONTSERRAT_28
#define LV_FONT_MONTSERRAT_28 1  /* screen title */
#undef  LV_FONT_MONTSERRAT_36
#define LV_FONT_MONTSERRAT_36 1  /* score card values */
/* LV_FONT_MONTSERRAT_14 (default font: sidebar log, dropdown) is already on. */

/* The core's config turns on Arm-2D accelerated rendering, but the Arm-2D
 * library isn't part of this build (LVGL 9.6+ fails to compile with it on). */
#undef  LV_USE_DRAW_ARM2D_SYNC
#define LV_USE_DRAW_ARM2D_SYNC 0

#endif /* SCOREBOARD_LV_CONF_H */
