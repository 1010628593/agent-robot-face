/* Headless hardware seam only; rendering/widgets use the real LVGL library. */
#ifndef BOT_HEADLESS_BSP_H
#define BOT_HEADLESS_BSP_H
#include "lvgl.h"
lv_indev_t *bsp_display_get_input_dev(void);
#endif
