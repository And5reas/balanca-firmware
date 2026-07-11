/**
 * @file wifi_manager.h
 * @brief Camada de gerenciamento: orquestra o scan e mantém o estado.
 *
 * Esta camada fica entre wifi_scan (dados brutos) e a User Action (UI).
 * Ela expõe uma API simples para a camada de apresentação e pode ser
 * reutilizada em outras telas sem depender do LVGL diretamente.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "wifi_scan.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Tipos                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Descrição formatada de uma rede, pronta para exibir na UI.
 *
 * Separa os dados "crus" (wifi_network_t) dos dados "formatados para
 * exibição" — evita formatar strings dentro da função de UI.
 *
 * Formato da description: "2.4 GHz • WPA2 • Excelente"
 */
typedef struct {
    const wifi_network_t *net;               /**< Ponteiro para dado bruto. */
    char description[48];                    /**< "2.4 GHz • WPA2 • Excelente", etc. */
} wifi_display_item_t;

/* ------------------------------------------------------------------ */
/* API pública                                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Executa um scan completo e armazena internamente os resultados.
 *
 * Após essa chamada, use wifi_manager_get_count() e
 * wifi_manager_get_item() para acessar os resultados.
 *
 * @return true se ao menos uma rede foi encontrada.
 */
bool wifi_manager_scan(void);

/**
 * @brief Retorna o número de redes encontradas no último scan.
 */
int wifi_manager_get_count(void);

/**
 * @brief Retorna os dados formatados do item no índice especificado.
 *
 * @param[in]  index  Índice de 0 a wifi_manager_get_count()-1.
 * @param[out] item   Preenchido com ponteiro e string formatada.
 * @return            true se o índice for válido.
 */
bool wifi_manager_get_item(int index, wifi_display_item_t *item);

/**
 * @brief Retorna os dados formatados do item no índice especificado.
 *
 * @param[in]  index  Índice de 0 a wifi_manager_get_count()-1.
 * @param[out] item   Preenchido com ponteiro e string formatada.
 * @return            true se o índice for válido.
 */
bool wifi_get_connected_ssid(char *ssid, int size);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
