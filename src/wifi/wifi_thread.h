/**
 * @file wifi_thread.h
 * @brief Contexto e estruturas para threading do scan Wi-Fi.
 *
 * Define as estruturas que são compartilhadas entre a thread de scan
 * e a thread principal do LVGL, permitindo passar resultados de forma
 * segura usando lv_async_call().
 *
 * Não deve conter chamadas diretas a LVGL — apenas estruturas de dados.
 */

#ifndef WIFI_THREAD_H
#define WIFI_THREAD_H

#include "wifi_manager.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* ------------------------------------------------------------------ */
    /* Tipos para comunicação entre threads                                */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Contexto do resultado do scan — passado entre threads.
     *
     * A thread de Wi-Fi preenche essa estrutura e passa via lv_async_call()
     * para a thread principal do LVGL atualizar a UI.
     *
     * Mantém um espelho dos dados para evitar race conditions.
     */
    typedef struct
    {
        /** Array de itens formatados prontos para exibir. */
        wifi_display_item_t items[WIFI_SCAN_MAX_NETWORKS];

        /** Número válido de itens em items[]. */
        int count;

        /** Flag indicando se o scan foi bem-sucedido. */
        bool success;

        /** Para possível expansão futura (mensagem de erro, etc.). */
        char error_msg[128];
    } wifi_scan_result_ui_t;

#define WIFI_SCAN_MAX_NETWORKS 20

    /* ------------------------------------------------------------------ */
    /* Funções de threading — internas de action_search_wifi.c            */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Função que roda em thread secundária para fazer o scan.
     *
     * Não deve chamar funções do LVGL diretamente.
     * Chama wifi_manager_scan() (bloqueante).
     * Ao final, chama lv_async_call(update_wifi_ui) para sincronizar
     * a atualização na thread principal.
     *
     * @param arg Ignorado. Pode ser NULL.
     * @return NULL sempre.
     */
    void *wifi_scan_thread(void *arg);

    /**
     * @brief Callback chamado pela thread principal do LVGL via lv_async_call().
     *
     * É seguro chamar aqui qualquer função do LVGL:
     * - lv_label_set_text()
     * - lv_obj_add_flag()
     * - lv_obj_remove_flag()
     * - etc.
     *
     * @param user_data Ponteiro para wifi_scan_result_ui_t (pode ser NULL).
     */
    void update_wifi_ui(void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_THREAD_H */
