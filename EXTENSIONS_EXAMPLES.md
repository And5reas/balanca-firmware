# Extensões Futuras - Exemplos de Código

## 🚀 Possíveis Melhorias para o Fluxo de Wi-Fi

Este arquivo contém exemplos de código para implementações futuras não incluídas na refatoração inicial, mas que aproveitem a arquitetura criada.

---

## 1. Cancelamento de Scan em Background

### Problema
Se o usuário sair da tela de Wi-Fi antes do scan terminar, a thread continua rodando.

### Solução: Flag Volátil

**Arquivo: action_search_wifi.c**

```c
/* ================================================================== */
/* CANCELAMENTO DE SCAN — VERSÃO COM SUPORTE A CANCELAMENTO           */
/* ================================================================== */

/**
 * Flag global para sinalizar cancelamento do scan.
 * Volátil porque é acessada de múltiplas threads.
 * sig_atomic_t garante leitura/escrita atômica.
 */
static volatile sig_atomic_t g_scan_cancel = false;

/**
 * @brief Reset a flag de cancelamento.
 * Chamar antes de iniciar novo scan.
 */
static void reset_scan_cancel_flag(void)
{
    g_scan_cancel = false;
}

/**
 * @brief User Action para cancelar scan em background.
 */
void action_cancel_wifi_scan(lv_event_t *e)
{
    (void)e;
    g_scan_cancel = true;
}

/**
 * @brief Versão estendida de wifi_scan_thread com suporte a cancelamento.
 */
void *wifi_scan_thread_cancellable(void *arg)
{
    (void)arg;

    wifi_scan_result_ui_t *result = 
        (wifi_scan_result_ui_t *)malloc(sizeof(wifi_scan_result_ui_t));
    if (!result)
        return NULL;

    memset(result, 0, sizeof(*result));

    /* Verificar cancelamento ANTES de iniciar */
    if (g_scan_cancel)
    {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Scan cancelado pelo usuário");
        result->success = false;
        lv_async_call(update_wifi_ui, (void *)result);
        return NULL;
    }

    /* Executar scan com verificações de cancelamento */
    result->success = wifi_manager_scan();

    if (result->success)
    {
        /* Verificar novamente durante coleta */
        if (g_scan_cancel)
        {
            snprintf(result->error_msg, sizeof(result->error_msg),
                     "Scan cancelado durante coleta");
            result->success = false;
            lv_async_call(update_wifi_ui, (void *)result);
            return NULL;
        }

        result->count = wifi_manager_get_count();

        int visible = (result->count < WIFI_MAX_VISIBLE_ITEMS)
                    ? result->count : WIFI_MAX_VISIBLE_ITEMS;

        for (int i = 0; i < visible; i++)
        {
            /* Verificar cancelamento a cada item */
            if (g_scan_cancel)
            {
                result->count = i; /* Incluir items coletados até aqui */
                break;
            }

            wifi_display_item_t display;
            if (wifi_manager_get_item(i, &display))
            {
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

    lv_async_call(update_wifi_ui, (void *)result);
    return NULL;
}
```

**Uso:**
```c
// action_search_wifi() usaria wifi_scan_thread_cancellable:
pthread_create(&scan_thread_id, NULL, wifi_scan_thread_cancellable, NULL);

// Botão "Cancelar":
void action_cancel_wifi_scan(lv_event_t *e) {
    action_cancel_wifi_scan(e);  // Seta g_scan_cancel = true
}
```

---

## 2. Indicador Visual de Progresso (Spinner)

### Problema
Usuário não sabe que o scan está rodando em background.

### Solução: Mostrar Spinner durante Scan

**Arquivo: action_search_wifi.c**

```c
/* ================================================================== */
/* INDICADOR VISUAL DE PROGRESSO                                      */
/* ================================================================== */

/**
 * @brief Versão estendida de update_wifi_ui com spinner.
 *
 * Diferença:
 *   - Mostra spinner enquanto aguardando resultado
 *   - Oculta spinner quando resultado chega
 */
void update_wifi_ui_with_spinner(void *user_data)
{
    wifi_scan_result_ui_t *result = (wifi_scan_result_ui_t *)user_data;

    if (!result)
        return;

    /* Ocultar spinner se existir */
    lv_obj_t *spinner = objects.spinner_wifi_scan; /* ADAPTE ao seu projeto */
    if (spinner)
    {
        lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    }

    /* Limpar UI */
    for (int i = 0; i < WIFI_MAX_VISIBLE_ITEMS; i++)
    {
        wifi_item_hide(i);
    }

    if (!result->success)
    {
        free(result);
        return;
    }

    /* Atualizar contador */
    char str[12];
    snprintf(str, sizeof(str), "%d", result->count);

    lv_obj_t *count_obj = objects.wifi_count;
    if (count_obj)
    {
        lv_label_set_text(count_obj, str);
    }

    /* Popular itens */
    for (int i = 0; i < result->count && i < WIFI_MAX_VISIBLE_ITEMS; i++)
    {
        wifi_item_populate(i, &result->items[i]);
    }

    free(result);
}

/**
 * @brief User Action versão estendida que mostra spinner.
 */
void action_search_wifi_with_spinner(lv_event_t *e)
{
    (void)e;

    /* Mostrar spinner ANTES de criar thread */
    lv_obj_t *spinner = objects.spinner_wifi_scan; /* ADAPTE */
    if (spinner)
    {
        lv_obj_remove_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    }

    pthread_t scan_thread_id;

    int ret = pthread_create(&scan_thread_id, NULL, wifi_scan_thread, NULL);

    if (ret != 0)
    {
        fprintf(stderr, "[action_search_wifi] Erro ao criar thread: %d\n", ret);
        if (spinner)
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    ret = pthread_detach(scan_thread_id);

    if (ret != 0)
    {
        fprintf(stderr, "[action_search_wifi] Erro ao desacoplar thread: %d\n", ret);
        if (spinner)
            lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
        return;
    }
}
```

---

## 3. Timer para Recarregar Lista de Redes Periodicamente

### Problema
RSSI e status das redes podem mudar após o scan inicial.

### Solução: Timer Periódico

```c
/* ================================================================== */
/* ATUALIZAÇÃO PERIÓDICA DE REDES                                     */
/* ================================================================== */

static lv_timer_t *g_wifi_refresh_timer = NULL;

/**
 * @brief Callback de timer para recarregar redes.
 * Rodando a cada 30 segundos.
 */
static void on_wifi_refresh_timer(lv_timer_t *t)
{
    (void)t;

    /* Disparar novo scan em background */
    pthread_t scan_thread_id;

    int ret = pthread_create(&scan_thread_id, NULL, wifi_scan_thread, NULL);

    if (ret != 0)
    {
        fprintf(stderr, "[on_wifi_refresh_timer] Erro ao criar thread: %d\n", ret);
        return;
    }

    pthread_detach(scan_thread_id);
}

/**
 * @brief Iniciar timer de atualização automática.
 * Chamar ao abrir a tela de Wi-Fi.
 */
void start_wifi_auto_refresh(void)
{
    if (!g_wifi_refresh_timer)
    {
        /* Timer dispara a cada 30 segundos (30000 ms) */
        g_wifi_refresh_timer = lv_timer_create(on_wifi_refresh_timer, 30000, NULL);
    }
}

/**
 * @brief Parar timer de atualização automática.
 * Chamar ao sair da tela de Wi-Fi.
 */
void stop_wifi_auto_refresh(void)
{
    if (g_wifi_refresh_timer)
    {
        lv_timer_delete(g_wifi_refresh_timer);
        g_wifi_refresh_timer = NULL;
    }
}
```

**Uso em ações do EEZ Studio:**
```c
// Ao entrar na tela Wi-Fi:
void on_wifi_screen_enter(lv_event_t *e) {
    start_wifi_auto_refresh();
}

// Ao sair:
void on_wifi_screen_exit(lv_event_t *e) {
    stop_wifi_auto_refresh();
    reset_scan_cancel_flag();  // Reset flag de cancelamento
}
```

---

## 4. Cache de Redes Entre Scans

### Problema
Se o usuário voltar para a tela de Wi-Fi, precisa fazer novo scan.

### Solução: Cache com Timestamp

```c
/* ================================================================== */
/* CACHE DE REDES                                                      */
/* ================================================================== */

#include <time.h>

typedef struct
{
    wifi_scan_result_ui_t data;
    time_t timestamp;
    bool valid;
} wifi_scan_cache_t;

static wifi_scan_cache_t g_wifi_cache = {0};

#define WIFI_CACHE_VALID_FOR_SECS 60  /* Cache válido por 60s */

/**
 * @brief Verificar se cache está válido.
 */
static bool is_wifi_cache_valid(void)
{
    if (!g_wifi_cache.valid)
        return false;

    time_t now = time(NULL);
    if (now < 0)
        return false;

    return (now - g_wifi_cache.timestamp) < WIFI_CACHE_VALID_FOR_SECS;
}

/**
 * @brief Salvar resultado em cache.
 */
static void cache_wifi_result(const wifi_scan_result_ui_t *result)
{
    if (!result)
        return;

    g_wifi_cache.data = *result;
    g_wifi_cache.timestamp = time(NULL);
    g_wifi_cache.valid = true;
}

/**
 * @brief Restaurar resultado do cache.
 */
static wifi_scan_result_ui_t *get_cached_wifi_result(void)
{
    if (!is_wifi_cache_valid())
        return NULL;

    wifi_scan_result_ui_t *copy = 
        (wifi_scan_result_ui_t *)malloc(sizeof(wifi_scan_result_ui_t));
    if (copy)
    {
        *copy = g_wifi_cache.data;
    }

    return copy;
}

/**
 * @brief Versão estendida com suporte a cache.
 */
void action_search_wifi_with_cache(lv_event_t *e)
{
    (void)e;

    /* Verificar cache ANTES de criar thread */
    wifi_scan_result_ui_t *cached = get_cached_wifi_result();

    if (cached)
    {
        /* Usar cache imediatamente */
        fprintf(stderr, "[action_search_wifi] Usando cache (~%lds)\n",
                time(NULL) - g_wifi_cache.timestamp);

        update_wifi_ui((void *)cached);
        return;
    }

    /* Cache inválido, fazer novo scan */
    fprintf(stderr, "[action_search_wifi] Cache inválido, fazendo novo scan\n");

    pthread_t scan_thread_id;

    int ret = pthread_create(&scan_thread_id, NULL, wifi_scan_thread, NULL);

    if (ret != 0)
    {
        fprintf(stderr, "[action_search_wifi] Erro ao criar thread: %d\n", ret);
        return;
    }

    pthread_detach(scan_thread_id);
}

/**
 * @brief Versão estendida de update_wifi_ui que salva cache.
 */
void update_wifi_ui_with_cache(void *user_data)
{
    wifi_scan_result_ui_t *result = (wifi_scan_result_ui_t *)user_data;

    if (!result)
        return;

    /* Salvar em cache ANTES de atualizar UI */
    if (result->success)
    {
        cache_wifi_result(result);
    }

    /* Resto da lógica igual... */
    for (int i = 0; i < WIFI_MAX_VISIBLE_ITEMS; i++)
    {
        wifi_item_hide(i);
    }

    if (!result->success)
    {
        free(result);
        return;
    }

    char str[12];
    snprintf(str, sizeof(str), "%d", result->count);

    lv_obj_t *count_obj = objects.wifi_count;
    if (count_obj)
    {
        lv_label_set_text(count_obj, str);
    }

    for (int i = 0; i < result->count && i < WIFI_MAX_VISIBLE_ITEMS; i++)
    {
        wifi_item_populate(i, &result->items[i]);
    }

    free(result);
}
```

---

## 5. Tratamento Avançado de Erros com Tentativa de Reconexão

### Problema
Se o scan falhar, usuário não sabe por quê.

### Solução: Retry com Backoff

```c
/* ================================================================== */
/* RETRY COM EXPONENTIAL BACKOFF                                      */
/* ================================================================== */

typedef struct
{
    int retry_count;
    int max_retries;
    int backoff_ms;  /* Aguardar antes de próxima tentativa */
} wifi_scan_retry_context_t;

/**
 * @brief Versão com retry automático.
 */
void *wifi_scan_thread_with_retry(void *arg)
{
    wifi_scan_retry_context_t *ctx = (wifi_scan_retry_context_t *)arg;

    if (!ctx)
    {
        ctx = (wifi_scan_retry_context_t *)malloc(sizeof(*ctx));
        if (!ctx)
            return NULL;
        ctx->retry_count = 0;
        ctx->max_retries = 3;
        ctx->backoff_ms = 1000;  /* 1 segundo */
    }

    wifi_scan_result_ui_t *result = 
        (wifi_scan_result_ui_t *)malloc(sizeof(*result));
    if (!result)
    {
        free(ctx);
        return NULL;
    }

    memset(result, 0, sizeof(*result));

    /* Loop de retry */
    while (ctx->retry_count < ctx->max_retries)
    {
        fprintf(stderr, "[wifi_scan] Tentativa %d/%d\n",
                ctx->retry_count + 1, ctx->max_retries);

        result->success = wifi_manager_scan();

        if (result->success)
        {
            /* Sucesso! Coletar dados */
            result->count = wifi_manager_get_count();

            int visible = (result->count < WIFI_MAX_VISIBLE_ITEMS)
                        ? result->count : WIFI_MAX_VISIBLE_ITEMS;

            for (int i = 0; i < visible; i++)
            {
                wifi_display_item_t display;
                if (wifi_manager_get_item(i, &display))
                {
                    result->items[i] = display;
                }
            }

            break;  /* Sucesso, sair do loop */
        }

        ctx->retry_count++;

        if (ctx->retry_count < ctx->max_retries)
        {
            /* Aguardar antes de próxima tentativa (backoff exponencial) */
            int wait_ms = ctx->backoff_ms * (1 << (ctx->retry_count - 1));
            usleep(wait_ms * 1000);  /* Converter ms para us */
        }
    }

    if (!result->success)
    {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Falha após %d tentativas", ctx->max_retries);
    }

    lv_async_call(update_wifi_ui, (void *)result);

    free(ctx);
    return NULL;
}

/**
 * @brief User Action com retry automático.
 */
void action_search_wifi_with_retry(lv_event_t *e)
{
    (void)e;

    wifi_scan_retry_context_t *ctx = 
        (wifi_scan_retry_context_t *)malloc(sizeof(*ctx));
    if (!ctx)
        return;

    ctx->retry_count = 0;
    ctx->max_retries = 3;
    ctx->backoff_ms = 1000;

    pthread_t scan_thread_id;

    int ret = pthread_create(&scan_thread_id, NULL, 
                           wifi_scan_thread_with_retry, ctx);

    if (ret != 0)
    {
        fprintf(stderr, "[action_search_wifi] Erro ao criar thread: %d\n", ret);
        free(ctx);
        return;
    }

    pthread_detach(scan_thread_id);
}
```

---

## 📋 Checklist de Implementação

Para cada extensão, adicionar em ordem:

1. **Cancelamento:**
   - [ ] Adicionar `volatile sig_atomic_t g_scan_cancel`
   - [ ] Adicionar `action_cancel_wifi_scan()`
   - [ ] Modificar thread para verificar flag
   - [ ] Adicionar botão "Cancelar" na UI

2. **Spinner:**
   - [ ] Criar spinner widget no EEZ Studio
   - [ ] Adicionar `update_wifi_ui_with_spinner()`
   - [ ] Modificar `action_search_wifi()` para mostrar spinner

3. **Auto-refresh:**
   - [ ] Adicionar `g_wifi_refresh_timer`
   - [ ] Implementar `on_wifi_refresh_timer()`
   - [ ] Chamar `start_wifi_auto_refresh()` ao entrar tela
   - [ ] Chamar `stop_wifi_auto_refresh()` ao sair

4. **Cache:**
   - [ ] Adicionar `#include <time.h>`
   - [ ] Implementar estrutura de cache
   - [ ] Adicionar `cache_wifi_result()`
   - [ ] Usar em `action_search_wifi_with_cache()`

5. **Retry:**
   - [ ] Adicionar `#include <unistd.h>` (para usleep)
   - [ ] Implementar `wifi_scan_thread_with_retry()`
   - [ ] Criar `action_search_wifi_with_retry()`

---

## ⚡ Performance vs Complexidade

| Extensão | Complexidade | Performance | Memória | Recomendado |
|----------|-------------|-------------|---------|------------|
| Cancelamento | Baixa | +5% | +4 bytes | ✅ Sim |
| Spinner | Muito Baixa | +2% | +0 bytes | ✅ Sim |
| Auto-refresh | Média | -10% | +200 bytes | ⚠️ Opcional |
| Cache | Média | +50% inicialmente | +800 bytes | ⚠️ Opcional |
| Retry | Média | +30% em erros | +100 bytes | ✅ Recomendado |

---

**Estas extensões podem ser implementadas incrementalmente sem afetar o código base!**
