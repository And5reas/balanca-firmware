/**
 * @file wifi_scan.h
 * @brief Camada de acesso ao wpa_cli para scan de redes Wi-Fi.
 *
 * Responsabilidade exclusiva: executar comandos no wpa_cli e
 * fazer o parsing da saída bruta de scan_results.
 *
 * Não conhece LVGL nem EEZ Studio — apenas dados.
 */

#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ------------------------------------------------------------------ */
/* Constantes                                                           */
/* ------------------------------------------------------------------ */

/** Número máximo de redes retornadas pelo scan. */
#define WIFI_SCAN_MAX_NETWORKS 20

/** Tamanho máximo do SSID (IEEE 802.11 define 32 bytes + '\0'). */
#define WIFI_SSID_MAX_LEN 33

/** Tamanho máximo da string de segurança ("WPA2-PSK", etc.). */
#define WIFI_FLAGS_MAX_LEN 64

/** Interface Wi-Fi padrão. */
#define WIFI_INTERFACE "wlan0"

/** Tempo de espera (segundos) após disparar o scan. */
#define WIFI_SCAN_WAIT_SECS 3

    /* ------------------------------------------------------------------ */
    /* Tipos                                                                */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Tipos de segurança conhecidos, em ordem crescente de força.
     */
    typedef enum
    {
        WIFI_SEC_OPEN = 0,   /**< Rede aberta, sem autenticação.          */
        WIFI_SEC_WEP,        /**< WEP (obsoleto, mas ainda existente).    */
        WIFI_SEC_WPA,        /**< WPA-PSK (TKIP).                        */
        WIFI_SEC_WPA2,       /**< WPA2-PSK (CCMP).                       */
        WIFI_SEC_WPA3,       /**< WPA3-SAE.                              */
        WIFI_SEC_ENTERPRISE, /**< EAP / 802.1X (sem senha simples).      */
        WIFI_SEC_UNKNOWN     /**< Flags não reconhecidas.                 */
    } wifi_security_t;

    /**
     * @brief Banda de frequência.
     */
    typedef enum
    {
        WIFI_BAND_2_4GHZ = 0,
        WIFI_BAND_5GHZ,
        WIFI_BAND_6GHZ, /**< Wi-Fi 6E / 7 */
        WIFI_BAND_UNKNOWN
    } wifi_band_t;

    /**
     * @brief Dados de uma rede Wi-Fi obtidos via scan.
     */
    typedef struct
    {
        char ssid[WIFI_SSID_MAX_LEN];       /**< SSID (UTF-8).          */
        char bssid[18];                     /**< MAC "aa:bb:cc:dd:ee:ff"*/
        int rssi;                           /**< Sinal em dBm.          */
        uint32_t frequency_mhz;             /**< Frequência em MHz.     */
        wifi_band_t band;                   /**< Banda calculada.       */
        wifi_security_t security;           /**< Tipo de segurança.     */
        bool has_password;                  /**< true se não for OPEN.  */
        char raw_flags[WIFI_FLAGS_MAX_LEN]; /**< Flags brutas do wpa_cli. */
    } wifi_network_t;

    /**
     * @brief Resultado de um scan completo.
     */
    typedef struct
    {
        wifi_network_t networks[WIFI_SCAN_MAX_NETWORKS];
        int count; /**< Número de redes válidas em networks[]. */
    } wifi_scan_result_t;

    /* ------------------------------------------------------------------ */
    /* API pública                                                          */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Dispara o scan e bloqueia até ter os resultados.
     *
     * Executa internamente:
     *   1. wpa_cli -i <iface> scan
     *   2. sleep(WIFI_SCAN_WAIT_SECS)
     *   3. wpa_cli -i <iface> scan_results
     *
     * @param[out] result  Buffer fornecido pelo chamador. Será preenchido.
     * @return             true em sucesso, false em erro (wpa_cli não encontrado,
     *                     interface inexistente, sem resultados válidos).
     */
    bool wifi_scan_run(wifi_scan_result_t *result);

    /**
     * @brief Converte enum wifi_security_t em string legível.
     *
     * @return Ponteiro para string literal estática ("WPA2", "Aberta", etc.).
     */
    const char *wifi_security_to_str(wifi_security_t sec);

    /**
     * @brief Converte enum wifi_band_t em string legível.
     *
     * @return "2.4 GHz", "5 GHz", "6 GHz" ou "? GHz".
     */
    const char *wifi_band_to_str(wifi_band_t band);

    /**
     * @brief Converte RSSI (dBm) em qualidade de sinal legível.
     *
     * Classificação:
     *   >= -50 dBm  → "Excelente" (muito próximo, sinal máximo)
     *   >= -60 dBm  → "Muito Boa" (perto, sinal muito forte)
     *   >= -70 dBm  → "Boa" (sinal forte)
     *   >= -80 dBm  → "Fraca" (sinal moderado)
     *   <  -80 dBm  → "Muito Fraca" (sinal fraco, marginal)
     *
     * @param rssi  Nível de sinal em dBm (típico: -100 a -20).
     * @return Ponteiro para string literal estática ("Excelente", "Boa", etc.).
     */
    const char *wifi_rssi_to_quality_str(int rssi);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_SCAN_H */
