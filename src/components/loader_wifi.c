#include "loader_wifi.h"

void wifi_loading_show(void)
{
    if(wifi_loading_overlay) {
        return;
    }

    wifi_loading_overlay = lv_obj_create(lv_screen_active());

    lv_obj_remove_style_all(wifi_loading_overlay);

    lv_obj_set_size(
        wifi_loading_overlay,
        LV_PCT(100),
        LV_PCT(100)
    );

    lv_obj_set_style_bg_color(
        wifi_loading_overlay,
        lv_color_black(),
        0
    );

    lv_obj_set_style_bg_opa(
        wifi_loading_overlay,
        LV_OPA_60,
        0
    );

    lv_obj_center(wifi_loading_overlay);

    /* Caixa central */
    lv_obj_t *panel = lv_obj_create(wifi_loading_overlay);

    lv_obj_set_size(panel, 240, 140);

    lv_obj_center(panel);

    lv_obj_set_style_radius(panel, 20, 0);

    lv_obj_set_style_bg_color(
        panel,
        lv_color_hex(0x284972),
        0
    );

    lv_obj_set_style_border_width(panel, 0, 0);

    /* Spinner */
    lv_obj_t *spinner = lv_spinner_create(
        panel
    );

    lv_obj_set_size(spinner, 40, 40);

    lv_obj_align(
        spinner,
        LV_ALIGN_TOP_MID,
        0,
        15
    );

    /* Texto */
    lv_obj_t *label = lv_label_create(panel);

    lv_label_set_text(
        label,
        "Procurando redes Wi-Fi..."
    );

    lv_obj_set_style_text_align(
        label,
        LV_TEXT_ALIGN_CENTER,
        0
    );

    lv_obj_align(
        label,
        LV_ALIGN_BOTTOM_MID,
        0,
        -25
    );
}

void wifi_loading_hide(void)
{
    if(wifi_loading_overlay) {
        lv_obj_delete(wifi_loading_overlay);
        wifi_loading_overlay = NULL;
    }
}