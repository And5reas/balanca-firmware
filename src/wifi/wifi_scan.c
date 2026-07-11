/**
 * @file wifi_scan.c
 * @brief Implementação do scan Wi-Fi via wpa_cli.
 *
 * Parsing da saída de "wpa_cli scan_results":
 *
 *   bssid / frequency / signal level / flags / ssid
 *   aa:bb:cc:dd:ee:ff   2437    -55    [WPA2-PSK-CCMP][ESS]   MinhaRede
 *   ...
 *
 * A primeira linha é o cabeçalho e deve ser ignorada.
 * Os campos são separados por TAB.
 */

#include "wifi_scan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* sleep() */
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* Helpers internos — não exportados                                    */
/* ------------------------------------------------------------------ */

/**
 * Deriva a banda a partir da frequência em MHz.
 * 2400-2500  → 2.4 GHz
 * 5000-5900  → 5 GHz
 * 5925-7125  → 6 GHz  (Wi-Fi 6E)
 */
static wifi_band_t freq_to_band(uint32_t freq_mhz)
{
    if (freq_mhz >= 2400 && freq_mhz <= 2500)
        return WIFI_BAND_2_4GHZ;
    if (freq_mhz >= 5000 && freq_mhz <= 5925)
        return WIFI_BAND_5GHZ;
    if (freq_mhz >= 5925 && freq_mhz <= 7125)
        return WIFI_BAND_6GHZ;
    return WIFI_BAND_UNKNOWN;
}

/**
 * Extrai o tipo de segurança a partir das flags brutas do wpa_cli.
 *
 * Ordem de checagem (do mais forte para o mais fraco):
 *   WPA3-SAE  → WIFI_SEC_WPA3
 *   WPA2      → WIFI_SEC_WPA2
 *   WPA       → WIFI_SEC_WPA
 *   WEP       → WIFI_SEC_WEP
 *   802.1X    → WIFI_SEC_ENTERPRISE
 *   [ESS]     → WIFI_SEC_OPEN
 */
static wifi_security_t parse_security(const char *flags)
{
    if (!flags || flags[0] == '\0')
        return WIFI_SEC_OPEN;

    /* WPA3 — deve vir antes de WPA2 para não dar falso positivo */
    if (strstr(flags, "SAE") != NULL)
        return WIFI_SEC_WPA3;

    if (strstr(flags, "WPA2") != NULL)
        return WIFI_SEC_WPA2;
    if (strstr(flags, "WPA") != NULL)
        return WIFI_SEC_WPA;
    if (strstr(flags, "WEP") != NULL)
        return WIFI_SEC_WEP;

    /* 802.1X / EAP (redes corporativas) */
    if (strstr(flags, "EAP") != NULL)
        return WIFI_SEC_ENTERPRISE;

    /* Redes abertas típicas só têm [ESS] ou [ESS][P2P] */
    if (strstr(flags, "ESS") != NULL)
        return WIFI_SEC_OPEN;

    return WIFI_SEC_UNKNOWN;
}

/**
 * Remove espaços e tabs do início e fim de uma string (in-place).
 */
static void str_trim(char *s)
{
    if (!s)
        return;

    /* Trim direita */
    int len = (int)strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                       s[len - 1] == '\n' || s[len - 1] == '\r'))
    {
        s[--len] = '\0';
    }

    /* Trim esquerda */
    int start = 0;
    while (s[start] == ' ' || s[start] == '\t')
        start++;
    if (start > 0)
        memmove(s, s + start, len - start + 1);
}

/**
 * Faz o parse de UMA linha de scan_results e preenche `net`.
 *
 * Formato esperado (campos separados por \t):
 *   BSSID \t FREQ \t SIGNAL \t FLAGS \t SSID
 *
 * @return true se a linha foi parseada com sucesso.
 */
static bool parse_scan_line(const char *line, wifi_network_t *net)
{
    /* Cópia mutável para strtok_r */
    char buf[256];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *token = NULL;

    /* Campo 1: BSSID */
    token = strtok_r(buf, "\t", &saveptr);
    if (!token)
        return false;
    str_trim(token);
    strncpy(net->bssid, token, sizeof(net->bssid) - 1);

    /* Campo 2: Frequência (MHz) */
    token = strtok_r(NULL, "\t", &saveptr);
    if (!token)
        return false;
    net->frequency_mhz = (uint32_t)atoi(token);
    net->band = freq_to_band(net->frequency_mhz);

    /* Campo 3: RSSI (signal level) */
    token = strtok_r(NULL, "\t", &saveptr);
    if (!token)
        return false;
    net->rssi = atoi(token);

    /* Campo 4: Flags de segurança */
    token = strtok_r(NULL, "\t", &saveptr);
    if (!token)
        return false;
    str_trim(token);
    strncpy(net->raw_flags, token, sizeof(net->raw_flags) - 1);
    net->security = parse_security(token);
    net->has_password = (net->security != WIFI_SEC_OPEN);

    /* Campo 5: SSID (pode conter espaços — pegar o restante) */
    token = strtok_r(NULL, "\t", &saveptr);
    if (!token)
    {
        /* SSID oculto — rede hidden */
        strncpy(net->ssid, "(hidden)", sizeof(net->ssid) - 1);
    }
    else
    {
        str_trim(token);
        if (token[0] == '\0')
        {
            strncpy(net->ssid, "(hidden)", sizeof(net->ssid) - 1);
        }
        else
        {
            strncpy(net->ssid, token, sizeof(net->ssid) - 1);
        }
    }

    return true;
}

/* ------------------------------------------------------------------ */
/* API pública                                                          */
/* ------------------------------------------------------------------ */

bool wifi_scan_run(wifi_scan_result_t *result)
{
    if (!result)
        return false;

    memset(result, 0, sizeof(*result));

    /* --- 1. Disparar o scan --- */
    {
        char cmd[128];
        snprintf(cmd, sizeof(cmd),
                 "wpa_cli -i %s scan > /dev/null 2>&1", WIFI_INTERFACE);

        int ret = system(cmd);
        if (ret != 0)
        {
            /* wpa_cli retorna 0 mesmo em "OK", checar apenas falha fatal */
            fprintf(stderr, "[wifi_scan] Falha ao executar scan (ret=%d)\n", ret);
            /* Continua mesmo assim — wpa_supplicant pode já ter iniciado scan */
        }
    }

    /* --- 2. Aguardar o scan completar --- */
    sleep(WIFI_SCAN_WAIT_SECS);

    /* --- 3. Ler resultados --- */
    {
        char cmd[128];
        snprintf(cmd, sizeof(cmd),
                 "wpa_cli -i %s scan_results", WIFI_INTERFACE);

        FILE *fp = popen(cmd, "r");
        if (!fp)
        {
            fprintf(stderr, "[wifi_scan] Falha ao abrir pipe para wpa_cli\n");
            return false;
        }

        char line[512];
        bool first_line = true; /* Pular cabeçalho */
        int count = 0;

        while (fgets(line, sizeof(line), fp) != NULL &&
               count < WIFI_SCAN_MAX_NETWORKS)
        {
            /* Ignorar linha de cabeçalho:
               "bssid / frequency / signal level / flags / ssid" */
            if (first_line)
            {
                first_line = false;
                continue;
            }

            /* Ignorar linhas vazias */
            str_trim(line);
            if (line[0] == '\0')
                continue;

            wifi_network_t net;
            memset(&net, 0, sizeof(net));

            if (parse_scan_line(line, &net))
            {
                result->networks[count++] = net;
            }
        }

        pclose(fp);
        result->count = count;
    }

    return (result->count > 0);
}

const char *wifi_security_to_str(wifi_security_t sec)
{
    switch (sec)
    {
    case WIFI_SEC_OPEN:
        return "Aberta";
    case WIFI_SEC_WEP:
        return "WEP";
    case WIFI_SEC_WPA:
        return "WPA";
    case WIFI_SEC_WPA2:
        return "WPA2";
    case WIFI_SEC_WPA3:
        return "WPA3";
    case WIFI_SEC_ENTERPRISE:
        return "EAP";
    default:
        return "?";
    }
}

const char *wifi_band_to_str(wifi_band_t band)
{
    switch (band)
    {
    case WIFI_BAND_2_4GHZ:
        return "2.4 GHz";
    case WIFI_BAND_5GHZ:
        return "5 GHz";
    case WIFI_BAND_6GHZ:
        return "6 GHz";
    default:
        return "? GHz";
    }
}

const char *wifi_rssi_to_quality_str(int rssi)
{
    if (rssi >= -50)
        return "Excelente";
    if (rssi >= -60)
        return "Muito Boa";
    if (rssi >= -70)
        return "Boa";
    if (rssi >= -80)
        return "Fraca";
    return "Muito Fraca";
}
