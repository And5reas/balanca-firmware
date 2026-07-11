
#ifndef LOADER_WIFI_H
#define LOADER_WIFI_H

#include "lvgl/lvgl.h"

static lv_obj_t *wifi_loading_overlay = NULL;

void wifi_loading_show(void);
void wifi_loading_hide(void);

#endif /* WIFI_SCREEN_H */