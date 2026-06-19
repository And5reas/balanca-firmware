#include "ui/actions.h"
#include "ui/ui.h"

void action_go_to_wifi_config_screen(lv_event_t *e) {
    // TODO: Implement action go_to_wifi_config_screen here
    LV_UNUSED(e);

    loadScreen(SCREEN_ID_WIFI_CONFIG_SCREEN);
}

void action_go_to_main(lv_event_t *e) {
    // TODO: Implement action go_to_main here
    LV_UNUSED(e);

    loadScreen(SCREEN_ID_MAIN);
}