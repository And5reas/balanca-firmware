#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_WIFI_CONFIG_SCREEN = 2,
    _SCREEN_ID_LAST = 2
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *wifi_config_screen;
    lv_obj_t *camera_lvgl;
    lv_obj_t *obj0;
    lv_obj_t *btn_back;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
    lv_obj_t *obj3;
    lv_obj_t *obj4;
    lv_obj_t *obj5;
    lv_obj_t *show_is_device_connect;
    lv_obj_t *ssid_wifi_connected;
    lv_obj_t *details_wifi_connection;
    lv_obj_t *obj6;
    lv_obj_t *wifi_count;
    lv_obj_t *btn_refresh;
    lv_obj_t *obj7;
    lv_obj_t *list_wifi;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_wifi_config_screen();
void tick_screen_wifi_config_screen();

void create_user_widget_wifi_item(lv_obj_t *parent_obj, int startWidgetIndex);
void tick_user_widget_wifi_item(int startWidgetIndex);

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/