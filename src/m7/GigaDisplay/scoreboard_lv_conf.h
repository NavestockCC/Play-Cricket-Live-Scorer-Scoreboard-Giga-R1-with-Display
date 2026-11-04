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
 *
 * PlatformIO can't tell that LVGL depends on this file (it's included via
 * that macro), so editing it doesn't rebuild LVGL -- run a clean build
 * (`pio run -t clean`) afterwards.
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

/* LVGL 9.6 deprecated two options the core's config still sets, warning once
 * per compiled file. Same settings under the current names: */
/* Matches the core's LV_COLOR_DEPTH 16, which stays defined -- setting this
 * stops LVGL deriving it from LV_COLOR_DEPTH (the deprecated path). */
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
/* Only included <stdint.h>; the core's LV_ASSERT_HANDLER is still used. */
#undef  LV_ASSERT_HANDLER_INCLUDE

#endif /* SCOREBOARD_LV_CONF_H */
