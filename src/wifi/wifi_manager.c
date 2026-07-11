/**
 * @file wifi_manager.c
 * @brief Implementação do gerenciador Wi-Fi.
 *
 * Mantém o resultado do último scan em memória estática e formata
 * as strings de exibição. Não conhece LVGL.
 */

#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Estado interno estático                                              */
/* ------------------------------------------------------------------ */

/** Resultado do último scan — buffer fixo, sem malloc. */
static wifi_scan_result_t s_last_scan;

/** Flag indicando se há dados válidos. */
static bool s_has_data = false;

/* ------------------------------------------------------------------ */
/* Helpers internos                                                     */
/* ------------------------------------------------------------------ */

/**
 * Monta a string de descrição no formato "2.4 GHz • WPA2 • Excelente".
 * Inclui banda, segurança e qualidade do sinal.
 */
static void build_description(const wifi_network_t *net, char *buf, int buf_size)
{
    snprintf(buf, (size_t)buf_size,
        "%s · %s · %s",  /* "banda • segurança • qualidade" */
        wifi_band_to_str(net->band),
        wifi_security_to_str(net->security),
        wifi_rssi_to_quality_str(net->rssi));
}

/* ------------------------------------------------------------------ */
/* API pública                                                          */
/* ------------------------------------------------------------------ */

bool wifi_manager_scan(void)
{
    s_has_data = false;
    memset(&s_last_scan, 0, sizeof(s_last_scan));

    bool ok = wifi_scan_run(&s_last_scan);
    if (ok)
    {
        s_has_data = true;
    }
    return ok;
}

int wifi_manager_get_count(void)
{
    return s_has_data ? s_last_scan.count : 0;
}

bool wifi_manager_get_item(int index, wifi_display_item_t *item)
{
    if (!item)
        return false;
    if (!s_has_data)
        return false;
    if (index < 0 || index >= s_last_scan.count)
        return false;

    item->net = &s_last_scan.networks[index];
    build_description(item->net, item->description, sizeof(item->description));

    return true;
}

bool wifi_get_connected_ssid(char *ssid, int size)
{
    FILE *fp = popen("wpa_cli status", "r");
    if (!fp)
        return false;

    char line[256];

    while (fgets(line, sizeof(line), fp)) {

        if (strncmp(line, "ssid=", 5) == 0) {

            strncpy(ssid, line + 5, size - 1);
            ssid[size - 1] = '\0';

            char *nl = strchr(ssid, '\n');
            if (nl)
                *nl = '\0';

            pclose(fp);
            return true;
        }
    }

    pclose(fp);
    return false;
}