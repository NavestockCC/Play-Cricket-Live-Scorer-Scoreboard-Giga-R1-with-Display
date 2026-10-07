#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// Tell LVGL not to load the default framework configuration settings
#define LV_CONF_SKIP

/* Enable Montserrat fonts */
#define LV_USE_FONT_MONTSERRAT_14    1
#define LV_USE_FONT_MONTSERRAT_16    1
#define LV_USE_FONT_MONTSERRAT_28    1

/* Set the default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif /*LV_CONF_H*/