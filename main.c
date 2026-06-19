#include "lvgl/lvgl.h"
#include <unistd.h>
#include <stdio.h>
#include "src/ui/ui.h"

/* Avisa o compilador que essa função existe em outro arquivo */
extern void lv_linux_disp_init(void);

/* Callback opcional – executado quando o WiFi conectar */
static void on_wifi_connected(const char *ssid, const char *ip)
{
    LV_LOG_USER("WiFi conectado: %s, IP: %s", ssid, ip);
    // Aqui você pode, por exemplo, esconder a tela de WiFi e mostrar a tela principal.
    // Exemplo: lv_obj_add_flag(lv_layer_top(), LV_OBJ_FLAG_HIDDEN);
}

int main(void) {
    /* Inicializa a biblioteca LVGL */
    lv_init();
    /* Cria e configura o display via Framebuffer do Linux */
    lv_linux_disp_init();

    ui_init();

    /* Loop infinito do LVGL */
    while(1) {
        lv_timer_handler(); /* Processa a interface */
        usleep(5000);       /* Pausa 5ms para não travar a CPU da Pi Zero */
    }

    return 0;
}