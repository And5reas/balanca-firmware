
#ifndef WIFI_SCREEN_H
#define WIFI_SCREEN_H

#include "lvgl/lvgl.h"

/**
 * wifi_screen.h / wifi_screen.c
 *
 * Tela de configuração WiFi para LVGL no RPi Zero 2 W.
 *
 * Funcionalidades:
 *  - Escaneia redes disponíveis via wpa_cli
 *  - Lista as redes em uma tabela com sinal (dBm) e cadeado (WPA/WEP/Open)
 *  - Teclado virtual para digitar a senha
 *  - Conecta via wpa_supplicant
 *  - Salva credenciais em /etc/wpa_supplicant/wpa_supplicant.conf para reconectar no boot
 *  - Callback chamado após conexão bem-sucedida
 *
 * Uso:
 *   wifi_screen_create(lv_screen_active(), my_on_connected_cb);
 */

/* Callback chamado quando a conexão é estabelecida com sucesso */
typedef void (*wifi_connected_cb_t)(const char *ssid, const char *ip);

/**
 * Cria a tela de WiFi sobre o parent fornecido.
 * @param parent  normalmente lv_screen_active()
 * @param cb      função chamada após conexão bem-sucedida (pode ser NULL)
 */
void wifi_screen_create(lv_obj_t *parent, wifi_connected_cb_t cb);

#endif /* WIFI_SCREEN_H */