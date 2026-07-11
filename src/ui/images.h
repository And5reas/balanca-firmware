#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_balanca_20;
extern const lv_img_dsc_t img_seta_back_20;
extern const lv_img_dsc_t img_wifi_20;
extern const lv_img_dsc_t img_refresh_20;
extern const lv_img_dsc_t img_wifi_28;
extern const lv_img_dsc_t img_signal_20;
extern const lv_img_dsc_t img_lock_20;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[7];

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/