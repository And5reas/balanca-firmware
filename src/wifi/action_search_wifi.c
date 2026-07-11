/**
 * @file action_search_wifi.c
 * @brief User Action do EEZ Studio + thread de scan Wi-Fi não-bloqueante.
 *
 * ========================================================================
 * ARQUITETURA: THREAD SECUNDÁRIA + CALLBACK DO LVGL
 * ========================================================================
 *
 * Fluxo:
 *
 *   action_search_wifi (thread LVGL)
 *      └── pthread_create(wifi_scan_thread)
 *      └── pthread_detach()
 *      └── retorna imediatamente (UI não congela)
 *
 *   wifi_scan_thread (thread secundária)
 *      └── wifi_manager_scan()  [BLOQUEANTE ~3s]
 *      └── Coleta resultados em wifi_scan_result_ui_t
 *      └── lv_async_call(update_wifi_ui, resultado)
 *      └── retorna
 *
 *   update_wifi_ui (thread LVGL - callback)
 *      └── Atualiza widgets LVGL
 *      └── Popula lista de Wi-Fi
 *      └── Mostra/oculta items
 *      └── retorna
 *
 * ========================================================================
 * SEGURANÇA E SINCRONIZAÇÃO
 * ========================================================================
 *
 * - wifi_scan_thread() é totalmente desacoplada de LVGL.
 * - Não há compartilhamento de estruturas mutáveis entre threads.
 * - O resultado é copiado para um novo buffer antes de chamar lv_async_call().
 * - lv_async_call() garante execução na thread principal do LVGL.
 * - Nenhuma lv_obj_t* é acessada fora da thread LVGL.
 *
 * ========================================================================
 */

/* ------------------------------------------------------------------ */
/* Includes                                                             */
/* ------------------------------------------------------------------ */

#include "../ui/actions.h"   /* Gerado pelo EEZ Studio */
#include "../ui/ui.h"        /* Gerado pelo EEZ Studio */
#include "../ui/vars.h"      /* Gerado pelo EEZ Studio (se existir) */

#include "../components/wifi_item.h"
#include "../components/loader_wifi.h"
#include "../components/select_wifi.h"

#include "wifi_manager.h"
#include "wifi_thread.h"

#include "lvgl/lvgl.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

/* ================================================================== */
/* THREAD SECUNDÁRIA — Executa o scan bloqueante                      */
/* ================================================================== */

/**
 * @brief Função que executa em thread secundária.
 *
 * Responsabilidade:
 *   1. Ocultar todos os items do pool (preparar state).
 *   2. Executar wifi_manager_scan() [BLOQUEANTE].
 *   3. Coletar resultados em uma estrutura temporária.
 *   4. Chamar lv_async_call() para sincronizar atualização na thread LVGL.
 *
 * Não deve acessar objetos LVGL diretamente — apenas chamar lv_async_call().
 *
 * @param arg Ignorado (passado por pthread_create, NULL neste caso).
 * @return NULL.
 */
void *wifi_scan_thread(void *arg)
{
    (void)arg; /* Parâmetro não utilizado */

    create_wifi_select_none(objects.details_wifi_connection);
    wifi_loading_show();

    /*
     * Preparar buffer para coletar resultados.
     * Este buffer será passado para update_wifi_ui() via lv_async_call().
     */
    wifi_scan_result_ui_t *result = (wifi_scan_result_ui_t *)malloc(sizeof(wifi_scan_result_ui_t));
    if (!result)
    {
        return NULL;
    }

    memset(result, 0, sizeof(*result));

    /* ---- Executar o scan (BLOQUEANTE ~3s) ---- */
    result->success = wifi_manager_scan();

    if (result->success)
    {
        /* Coletar resultados formatados */
        result->count = wifi_manager_get_count();

        for (int i = 0; i < result->count; i++)
        {
            wifi_display_item_t display;
            if (wifi_manager_get_item(i, &display))
            {
                /* Copiar para o resultado — evita lifetime issues */
                result->items[i] = display;
            }
        }
    }
    else
    {
        result->count = 0;
        snprintf(result->error_msg, sizeof(result->error_msg),
            "Nenhuma rede encontrada");
    }

    /*
     * Agendar a atualização da UI na thread principal do LVGL.
     * lv_async_call() é thread-safe e copiará o ponteiro para fila interna.
     */
    lv_async_call(update_wifi_ui, (void *)result);

    return NULL;
}

/* ================================================================== */
/* CALLBACK DO LVGL — Atualiza a UI (thread principal)                */
/* ================================================================== */

/**
 * @brief Callback chamado pela thread principal do LVGL via lv_async_call().
 *
 * Execução garantida na thread principal — é SEGURO chamar funções LVGL aqui.
 *
 * Responsabilidade:
 *   1. Ocultar todos os WifiItems não utilizados.
 *   2. Popular os itens com dados do resultado.
 *   3. Atualizar label de contagem.
 *   4. Liberar memória do resultado.
 *
 * @param user_data Ponteiro para wifi_scan_result_ui_t (alocado por wifi_scan_thread).
 */
void update_wifi_ui(void *user_data)
{
    wifi_scan_result_ui_t *result = (wifi_scan_result_ui_t *)user_data;

    if (!result)
    {
        return;
    }

    if (!result->success)
    {
        /*
         * Scan falhou — exibir mensagem de erro (opcional).
         * Exemplo:
         *   lv_label_set_text(objects.label_error, result->error_msg);
         */
        free(result);
        return;
    }

    /* ---- Atualizar label de contagem ---- */
    {
        char str[12];
        snprintf(str, sizeof(str), "%d", result->count);

        lv_obj_t *count_obj = objects.wifi_count;
        if (count_obj)
        {
            lv_label_set_text(count_obj, str);
        }
    }

    lv_obj_t *obj = objects.list_wifi;
    lv_obj_clean(obj);
    /* ---- Popular os itens visíveis ---- */
    for (int i = 0; i < result->count; i++)
    {
        const wifi_display_item_t *display = &result->items[i];

        lv_obj_t *base = lv_obj_create(obj);
        lv_obj_set_pos(base, 286, 128);
        lv_obj_set_size(base, 414, 65);
        lv_obj_set_style_pad_left(base, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(base, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(base, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(base, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(base, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(base, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        create_wifi_item(base, display);
    }

    wifi_loading_hide();

    /* Liberar memória alocada por wifi_scan_thread */
    free(result);
}

/* ================================================================== */
/* USER ACTION DO EEZ STUDIO                                           */
/* ================================================================== */

/**
 * @brief Action chamada quando o usuário pressiona o botão de scan Wi-Fi.
 *
 * Responsabilidade ÚNICA: criar uma thread secundária e retornar imediatamente.
 *
 * Fluxo:
 *   1. Criar thread com pthread_create(wifi_scan_thread).
 *   2. Desacloplar com pthread_detach().
 *   3. Retornar — UI não congela.
 *   4. A thread cuida do resto (scan bloqueante + atualização via lv_async_call).
 *
 * @param e Evento LVGL (não utilizado nesta implementação).
 */
void action_search_wifi(lv_event_t *e)
{
    (void)e; /* Parâmetro não utilizado */

    pthread_t scan_thread_id;

    /*
     * Criar thread secundária que executará wifi_scan_thread().
     *
     * - Atributos: NULL (usa defaults)
     * - Função: wifi_scan_thread
     * - Argumento: NULL
     *
     * pthread_create() copia o ID em scan_thread_id mas não bloqueia.
     */
    int ret = pthread_create(&scan_thread_id, NULL, wifi_scan_thread, NULL);

    if (ret != 0)
    {
        fprintf(stderr, "[action_search_wifi] Erro ao criar thread: %d\n", ret);
        return;
    }

    /*
     * Desacoplar a thread — permite que ela seja limpa automaticamente
     * quando terminar, sem precisar de pthread_join().
     *
     * Importante: pthread_detach() recebe pthread_t (o ID),
     * NÃO um ponteiro para função.
     */
    ret = pthread_detach(scan_thread_id);

    if (ret != 0)
    {
        fprintf(stderr, "[action_search_wifi] Erro ao desacoplar thread: %d\n", ret);
        return;
    }

    /*
     * A thread está criada e desacoplada.
     * Essa função retorna imediatamente.
     * A UI continua responsiva enquanto o scan roda em background.
     */
}

/* ================================================================== */
/* MELHORIAS FUTURAS                                                   */
/* ================================================================== */

/*
 * Possíveis expansões:
 *
 * 1. Cancelar scan em background:
 *    - Adicionar flag global `volatile sig_atomic_t g_scan_cancel = false;`
 *    - Na thread, checar a flag periodicamente.
 *    - Ação "Cancelar" seta a flag.
 *
 * 2. Barra de progresso durante o scan:
 *    - Usar lv_timer_t para atualizar animação a cada 500ms.
 *    - Barra de progresso indeterminada (spinner) enquanto aguarda.
 *
 * 3. Tratamento de erros mais elaborado:
 *    - Exibir mensagens em um label da UI.
 *    - Tentar reconexão automática.
 *
 * 4. Atualização de RSSI em tempo real:
 *    - Monitorar redes visíveis mesmo após scan.
 *    - Timer que chama wifi_manager_get_item() periodicamente.
 *
 * 5. Integração com wpa_supplicant events:
 *    - Usar wpa_supplicant D-Bus interface para notificações de scan.
 *    - Mais eficiente que polling.
 */
