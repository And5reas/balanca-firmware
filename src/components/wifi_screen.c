#include "wifi_screen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ------------------------------------------------------------------ */
/*  Configurações                                                  */
/* ------------------------------------------------------------------ */
#define WIFI_IFACE          "wlan0"
#define WPA_CONF            "/etc/wpa_supplicant/wpa_supplicant.conf"
#define MAX_NETWORKS        20
#define SSID_MAX_LEN        64
#define PASS_MAX_LEN        64
#define SCAN_TIMEOUT_S      8
#define CONNECT_TIMEOUT_S   15

/* Paleta */
#define COLOR_BG            lv_color_hex(0x1A1A2E)
#define COLOR_PANEL         lv_color_hex(0x16213E)
#define COLOR_ACCENT        lv_color_hex(0x0F3460)
#define COLOR_PRIMARY       lv_color_hex(0x4CC9F0)
#define COLOR_SUCCESS       lv_color_hex(0x4CAF50)
#define COLOR_ERROR         lv_color_hex(0xE94560)
#define COLOR_TEXT          lv_color_hex(0xEEEEEE)
#define COLOR_TEXT_DIM      lv_color_hex(0x8899AA)
#define COLOR_ROW_SEL       lv_color_hex(0x0F3460)

/* Fallback para símbolos/flag ausentes em algumas compilações LVGL 9.5 */
#ifndef LV_SYMBOL_LOCK
#define LV_SYMBOL_LOCK      "\xEF\x80\xA3"   /* cadeado FontAwesome */
#endif
#ifndef LV_OBJ_FLAG_DISABLE
#define LV_OBJ_FLAG_DISABLE (1UL << 6)       /* bit 6 no LVGL 9 */
#endif

/* ------------------------------------------------------------------ */
/*  Estruturas internas                                               */
/* ------------------------------------------------------------------ */
typedef struct {
    char ssid[SSID_MAX_LEN];
    int  signal;
    int  encrypted;
} wifi_network_t;

typedef struct {
    wifi_network_t  networks[MAX_NETWORKS];
    int             net_count;
    int             selected_idx;
    char            password[PASS_MAX_LEN];
    wifi_connected_cb_t on_connected;

    lv_obj_t *screen;
    lv_obj_t *list_panel;
    lv_obj_t *table;
    lv_obj_t *btn_scan;
    lv_obj_t *btn_connect;
    lv_obj_t *status_label;

    lv_obj_t *pass_panel;
    lv_obj_t *pass_label;
    lv_obj_t *pass_ta;
    lv_obj_t *kb;
    lv_obj_t *btn_back;
    lv_obj_t *btn_ok;

    lv_obj_t *conn_panel;
    lv_obj_t *conn_label;
    lv_obj_t *conn_spinner;
    lv_obj_t *btn_conn_cancel;
} wifi_ctx_t;

static wifi_ctx_t *g_ctx = NULL;

/* ------------------------------------------------------------------ */
/*  Utilitários de sistema                                            */
/* ------------------------------------------------------------------ */
static int run_cmd(const char *cmd, char *out, size_t out_sz)
{
    FILE *fp = popen(cmd, "r");
    if(!fp) return -1;
    size_t total = 0;
    char line[256];
    while(fgets(line, sizeof(line), fp) && total < out_sz - 1) {
        size_t len = strlen(line);
        if(total + len < out_sz - 1) {
            memcpy(out + total, line, len);
            total += len;
        }
    }
    out[total] = '\0';
    return pclose(fp);
}

static int wifi_scan(void)
{
    char buf[4096] = {0};
    run_cmd("wpa_cli -i " WIFI_IFACE " scan 2>/dev/null", buf, sizeof(buf));
    sleep(SCAN_TIMEOUT_S);
    run_cmd("wpa_cli -i " WIFI_IFACE " scan_results 2>/dev/null", buf, sizeof(buf));

    g_ctx->net_count = 0;
    char *line = strtok(buf, "\n");
    int first = 1;
    while(line && g_ctx->net_count < MAX_NETWORKS) {
        if(first) { first = 0; line = strtok(NULL, "\n"); continue; }
        char bssid[32], freq[16], sig[16], flags[128], ssid[SSID_MAX_LEN];
        if(sscanf(line, "%31s %15s %15s %127s %63[^\n]",
                  bssid, freq, sig, flags, ssid) == 5) {
            wifi_network_t *n = &g_ctx->networks[g_ctx->net_count];
            strncpy(n->ssid, ssid, SSID_MAX_LEN - 1);
            n->ssid[SSID_MAX_LEN - 1] = '\0';
            n->signal    = atoi(sig);
            n->encrypted = (strstr(flags, "WPA") || strstr(flags, "WEP")) ? 1 : 0;
            g_ctx->net_count++;
        }
        line = strtok(NULL, "\n");
    }
    return g_ctx->net_count;
}

static const char *signal_icon(int dbm)
{
    if(dbm >= -55) return LV_SYMBOL_WIFI "+++";
    if(dbm >= -70) return LV_SYMBOL_WIFI "++";
    if(dbm >= -80) return LV_SYMBOL_WIFI "+";
    return LV_SYMBOL_WIFI "-";
}

static int wifi_save_and_connect(const char *ssid, const char *psk)
{
    FILE *fp = fopen(WPA_CONF, "w");
    if(!fp) return -1;
    fprintf(fp,
        "ctrl_interface=/var/run/wpa_supplicant\n"
        "ctrl_interface_group=0\n"
        "update_config=1\n"
        "country=BR\n\n"
        "network={\n"
        "    ssid=\"%s\"\n"
        "    psk=\"%s\"\n"
        "    key_mgmt=WPA-PSK\n"
        "    priority=1\n"
        "}\n", ssid, psk);
    fclose(fp);
    chmod(WPA_CONF, 0600);
    run_cmd("wpa_cli -i " WIFI_IFACE " reconfigure 2>/dev/null", NULL, 0);
    char buf[256];
    for(int i = 0; i < CONNECT_TIMEOUT_S; i++) {
        sleep(1);
        run_cmd("wpa_cli -i " WIFI_IFACE " status 2>/dev/null", buf, sizeof(buf));
        if(strstr(buf, "wpa_state=COMPLETED")) {
            run_cmd("ip addr flush dev " WIFI_IFACE " 2>/dev/null", NULL, 0);
            run_cmd("ip addr add 192.168.0.15/24 dev " WIFI_IFACE " 2>/dev/null", NULL, 0);
            run_cmd("ip route add default via 192.168.0.1 2>/dev/null", NULL, 0);
            run_cmd("echo 'nameserver 8.8.8.8' > /etc/resolv.conf", NULL, 0);
            return 0;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/*  Protótipos                                                        */
/* ------------------------------------------------------------------ */
static void show_list_panel(void);
static void show_pass_panel(const char *ssid);
static void show_conn_panel(const char *ssid);

/* ------------------------------------------------------------------ */
/*  Callbacks de timer                                                 */
/* ------------------------------------------------------------------ */
typedef struct { char ssid[SSID_MAX_LEN]; char psk[PASS_MAX_LEN]; } conn_args_t;

static void scan_timer_cb(lv_timer_t *t)
{
    lv_timer_delete(t);
    wifi_ctx_t *ctx = (wifi_ctx_t *)lv_timer_get_user_data(t);
    int count = wifi_scan();

    lv_table_set_row_count(ctx->table, count > 0 ? count : 1);
    if(count == 0) {
        lv_table_set_cell_value(ctx->table, 0, 0, LV_SYMBOL_WARNING " Nenhuma rede encontrada");
        lv_table_set_cell_value(ctx->table, 0, 1, "");
        lv_table_set_cell_value(ctx->table, 0, 2, "");
    } else {
        for(int i = 0; i < count; i++) {
            wifi_network_t *n = &ctx->networks[i];
            lv_table_set_cell_value(ctx->table, i, 0, n->ssid);
            lv_table_set_cell_value(ctx->table, i, 1, signal_icon(n->signal));
            lv_table_set_cell_value(ctx->table, i, 2, n->encrypted ? LV_SYMBOL_LOCK : "");
        }
    }

    lv_label_set_text(ctx->status_label,
        count > 0 ? "Toque em uma rede para conectar." : "Nenhuma rede encontrada.");
    lv_obj_remove_flag(ctx->btn_scan, LV_OBJ_FLAG_DISABLE);
    lv_obj_remove_flag(ctx->btn_connect, LV_OBJ_FLAG_DISABLE);
}

static void connect_timer_cb(lv_timer_t *t)
{
    lv_timer_delete(t);
    conn_args_t *args = (conn_args_t *)lv_timer_get_user_data(t);
    wifi_ctx_t  *ctx  = g_ctx;

    int ret = wifi_save_and_connect(args->ssid, args->psk);
    free(args);

    if(ret == 0) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 LV_SYMBOL_OK " Conectado!\n\nSSID: %s\nIP: 192.168.0.15",
                 ctx->networks[ctx->selected_idx].ssid);
        lv_label_set_text(ctx->conn_label, buf);
        lv_obj_set_style_text_color(ctx->conn_label, COLOR_SUCCESS, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(ctx->conn_spinner, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ctx->btn_conn_cancel, "Fechar");
        if(ctx->on_connected)
            ctx->on_connected(ctx->networks[ctx->selected_idx].ssid, "192.168.0.15");
    } else {
        lv_label_set_text(ctx->conn_label,
            LV_SYMBOL_CLOSE " Falha ao conectar.\n\nVerifique a senha e tente novamente.");
        lv_obj_set_style_text_color(ctx->conn_label, COLOR_ERROR, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(ctx->conn_spinner, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ctx->btn_conn_cancel, "Voltar");
    }
}

/* ------------------------------------------------------------------ */
/*  Callback de desenho da tabela (corrigido para LVGL 9.5)           */
/* ------------------------------------------------------------------ */
static void table_draw_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_task_t *draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *base = lv_draw_task_get_draw_dsc(draw_task);
    if(base->part != LV_PART_ITEMS) return;

    if(lv_draw_task_get_type(draw_task) != LV_DRAW_TASK_TYPE_FILL)
        return;

    lv_draw_rect_dsc_t *rect_dsc = (lv_draw_rect_dsc_t *)base;

    uint32_t row, col;
    lv_table_get_selected_cell(obj, &row, &col);
    uint32_t cur_row = base->id1;

    if(cur_row == row) {
        rect_dsc->bg_color = COLOR_ROW_SEL;
        rect_dsc->bg_opa   = LV_OPA_COVER;
    } else {
        rect_dsc->bg_color = (cur_row % 2 == 0) ? COLOR_PANEL : COLOR_BG;
        rect_dsc->bg_opa   = LV_OPA_COVER;
    }
}

/* ------------------------------------------------------------------ */
/*  Callbacks de UI                                                   */
/* ------------------------------------------------------------------ */
static void table_value_changed_cb(lv_event_t *e)
{
    wifi_ctx_t *ctx = (wifi_ctx_t *)lv_event_get_user_data(e);
    uint32_t row, col;
    lv_table_get_selected_cell(lv_event_get_target(e), &row, &col);
    if((int)row < ctx->net_count) {
        ctx->selected_idx = (int)row;
        char buf[64];
        snprintf(buf, sizeof(buf), "Selecionado: %s", ctx->networks[row].ssid);
        lv_label_set_text(ctx->status_label, buf);
        /* Força redesenho para atualizar as cores das linhas */
        lv_obj_invalidate(ctx->table);
    }
}

static void btn_scan_cb(lv_event_t *e)
{
    wifi_ctx_t *ctx = (wifi_ctx_t *)lv_event_get_user_data(e);
    lv_label_set_text(ctx->status_label, LV_SYMBOL_REFRESH " Escaneando redes...");
    lv_obj_add_flag(ctx->btn_scan, LV_OBJ_FLAG_DISABLE);
    lv_obj_add_flag(ctx->btn_connect, LV_OBJ_FLAG_DISABLE);
    lv_table_set_row_count(ctx->table, 1);
    lv_table_set_cell_value(ctx->table, 0, 0, "Escaneando...");
    lv_table_set_cell_value(ctx->table, 0, 1, "");
    lv_table_set_cell_value(ctx->table, 0, 2, "");
    lv_timer_create(scan_timer_cb, 100, ctx);
}

static void btn_connect_cb(lv_event_t *e)
{
    wifi_ctx_t *ctx = (wifi_ctx_t *)lv_event_get_user_data(e);
    if(ctx->selected_idx < 0 || ctx->selected_idx >= ctx->net_count) {
        lv_label_set_text(ctx->status_label, LV_SYMBOL_WARNING " Selecione uma rede.");
        return;
    }
    show_pass_panel(ctx->networks[ctx->selected_idx].ssid);
}

static void btn_back_cb(lv_event_t *e)   { show_list_panel(); }
static void btn_conn_cancel_cb(lv_event_t *e) { show_list_panel(); }

static void btn_ok_cb(lv_event_t *e)
{
    wifi_ctx_t *ctx = (wifi_ctx_t *)lv_event_get_user_data(e);
    const char *pass = lv_textarea_get_text(ctx->pass_ta);
    if(strlen(pass) < 8) {
        lv_label_set_text(ctx->pass_label,
            LV_SYMBOL_WARNING " Senha muito curta (mínimo 8 caracteres)");
        lv_obj_set_style_text_color(ctx->pass_label, COLOR_ERROR, LV_PART_MAIN | LV_STATE_DEFAULT);
        return;
    }
    strncpy(ctx->password, pass, PASS_MAX_LEN - 1);
    ctx->password[PASS_MAX_LEN - 1] = '\0';
    show_conn_panel(ctx->networks[ctx->selected_idx].ssid);
}

static void kb_event_cb(lv_event_t *e)
{
    lv_obj_t *kb = lv_event_get_target(e);
    lv_keyboard_def_event_cb(e);
    if(lv_event_get_code(e) == LV_EVENT_READY)
        btn_ok_cb(e);
}

/* ------------------------------------------------------------------ */
/*  Construção dos painéis                                            */
/* ------------------------------------------------------------------ */
static lv_obj_t *make_btn(lv_obj_t *parent, const char *text,
                           lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_style_bg_color(btn, COLOR_ACCENT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(btn, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(btn, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, COLOR_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl);
    if(cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}

static void show_list_panel(void)
{
    wifi_ctx_t *ctx = g_ctx;
    lv_obj_clear_flag(ctx->list_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ctx->pass_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ctx->conn_panel, LV_OBJ_FLAG_HIDDEN);
    ctx->selected_idx = -1;
    lv_label_set_text(ctx->status_label, "Toque em " LV_SYMBOL_REFRESH " para escanear redes.");
    lv_obj_invalidate(ctx->table); /* Força repintura */
}

static void show_pass_panel(const char *ssid)
{
    wifi_ctx_t *ctx = g_ctx;
    lv_obj_add_flag(ctx->list_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ctx->pass_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ctx->conn_panel, LV_OBJ_FLAG_HIDDEN);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s Senha para: %s", LV_SYMBOL_LOCK, ssid);
    lv_label_set_text(ctx->pass_label, buf);
    lv_obj_set_style_text_color(ctx->pass_label, COLOR_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_textarea_set_text(ctx->pass_ta, "");
    lv_obj_scroll_to(ctx->pass_panel, 0, 0, LV_ANIM_OFF);
}

static void show_conn_panel(const char *ssid)
{
    wifi_ctx_t *ctx = g_ctx;
    lv_obj_add_flag(ctx->list_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ctx->pass_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ctx->conn_panel, LV_OBJ_FLAG_HIDDEN);
    char buf[128];
    snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI " Conectando a:\n%s\n\nAguarde...", ssid);
    lv_label_set_text(ctx->conn_label, buf);
    lv_obj_set_style_text_color(ctx->conn_label, COLOR_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ctx->conn_spinner, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(ctx->btn_conn_cancel, "Cancelar");

    conn_args_t *args = malloc(sizeof(conn_args_t));
    strncpy(args->ssid, ssid, SSID_MAX_LEN - 1);
    args->ssid[SSID_MAX_LEN - 1] = '\0';
    strncpy(args->psk,  ctx->password, PASS_MAX_LEN - 1);
    args->psk[PASS_MAX_LEN - 1] = '\0';
    lv_timer_create(connect_timer_cb, 200, args);
}

/* ------------------------------------------------------------------ */
/*  Ponto de entrada público                                          */
/* ------------------------------------------------------------------ */
void wifi_screen_create(lv_obj_t *parent, wifi_connected_cb_t cb)
{
    g_ctx = calloc(1, sizeof(wifi_ctx_t));
    wifi_ctx_t *ctx = g_ctx;
    ctx->on_connected = cb;
    ctx->selected_idx = -1;

    /* Container raiz */
    ctx->screen = lv_obj_create(parent);
    lv_obj_set_size(ctx->screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ctx->screen, COLOR_BG, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ctx->screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ctx->screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ctx->screen, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ctx->screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Título */
    lv_obj_t *title = lv_label_create(ctx->screen);
    lv_label_set_text(title, LV_SYMBOL_WIFI "  Configuração WiFi");
    lv_obj_set_style_text_color(title, COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *line = lv_obj_create(ctx->screen);
    lv_obj_set_size(line, LV_PCT(96), 1);
    lv_obj_set_style_bg_color(line, COLOR_ACCENT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 40);

    /* PAINEL 1 – Lista */
    ctx->list_panel = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->list_panel, LV_PCT(100), LV_PCT(100) - 44);
    lv_obj_align(ctx->list_panel, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(ctx->list_panel, COLOR_BG, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ctx->list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ctx->list_panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ctx->list_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ctx->list_panel, LV_OBJ_FLAG_SCROLLABLE);

    ctx->table = lv_table_create(ctx->list_panel);
    lv_table_set_column_count(ctx->table, 3);
    lv_table_set_column_width(ctx->table, 0, 560);
    lv_table_set_column_width(ctx->table, 1, 100);
    lv_table_set_column_width(ctx->table, 2, 60);
    lv_obj_set_size(ctx->table, LV_PCT(100), 340);
    lv_obj_align(ctx->table, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(ctx->table, COLOR_PANEL, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ctx->table, COLOR_ACCENT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ctx->table, COLOR_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_table_set_cell_value(ctx->table, 0, 0, "Rede (SSID)");
    lv_table_set_cell_value(ctx->table, 0, 1, "Sinal");
    lv_table_set_cell_value(ctx->table, 0, 2, "");
    lv_obj_set_style_bg_color(ctx->table, COLOR_ACCENT, LV_PART_ITEMS | LV_STATE_DEFAULT);

    /* Eventos da tabela */
    lv_obj_add_event_cb(ctx->table, table_draw_cb, LV_EVENT_DRAW_TASK_ADDED, ctx);
    lv_obj_add_flag(ctx->table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(ctx->table, table_value_changed_cb, LV_EVENT_VALUE_CHANGED, ctx);

    ctx->status_label = lv_label_create(ctx->list_panel);
    lv_obj_set_width(ctx->status_label, LV_PCT(100));
    lv_label_set_long_mode(ctx->status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(ctx->status_label, COLOR_TEXT_DIM, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(ctx->status_label, LV_ALIGN_TOP_MID, 0, 350);
    lv_label_set_text(ctx->status_label, "Toque em " LV_SYMBOL_REFRESH " para escanear redes.");

    /* Linha de botões */
    lv_obj_t *btn_row = lv_obj_create(ctx->list_panel);
    lv_obj_set_size(btn_row, LV_PCT(100), 60);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    ctx->btn_scan = make_btn(btn_row, LV_SYMBOL_REFRESH " Escanear", btn_scan_cb, ctx);
    ctx->btn_connect = make_btn(btn_row, LV_SYMBOL_WIFI " Conectar", btn_connect_cb, ctx);
    lv_obj_set_style_bg_color(ctx->btn_connect, COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lv_obj_get_child(ctx->btn_connect, 0), COLOR_BG, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* PAINEL 2 – Senha */
    ctx->pass_panel = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->pass_panel, LV_PCT(100), LV_PCT(100) - 44);
    lv_obj_align(ctx->pass_panel, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(ctx->pass_panel, COLOR_BG, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ctx->pass_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ctx->pass_panel, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ctx->pass_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ctx->pass_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ctx->pass_panel, LV_OBJ_FLAG_SCROLLABLE);

    ctx->pass_label = lv_label_create(ctx->pass_panel);
    lv_obj_set_width(ctx->pass_label, LV_PCT(100));
    lv_obj_set_style_text_color(ctx->pass_label, COLOR_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ctx->pass_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(ctx->pass_label, LV_SYMBOL_LOCK " Senha da rede");
    lv_obj_align(ctx->pass_label, LV_ALIGN_TOP_MID, 0, 0);

    ctx->pass_ta = lv_textarea_create(ctx->pass_panel);
    lv_obj_set_size(ctx->pass_ta, LV_PCT(100), 50);
    lv_obj_align(ctx->pass_ta, LV_ALIGN_TOP_MID, 0, 36);
    lv_textarea_set_password_mode(ctx->pass_ta, true);
    lv_textarea_set_placeholder_text(ctx->pass_ta, "Digite a senha...");
    lv_textarea_set_one_line(ctx->pass_ta, true);
    lv_obj_set_style_bg_color(ctx->pass_ta, COLOR_PANEL, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ctx->pass_ta, COLOR_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ctx->pass_ta, COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_FOCUSED);

    ctx->kb = lv_keyboard_create(ctx->pass_panel);
    lv_keyboard_set_textarea(ctx->kb, ctx->pass_ta);
    lv_obj_set_size(ctx->kb, LV_PCT(100), 240);
    lv_obj_align(ctx->kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ctx->kb, COLOR_PANEL, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ctx->kb, COLOR_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ctx->kb, kb_event_cb, LV_EVENT_ALL, ctx);

    lv_obj_t *pass_btns = lv_obj_create(ctx->pass_panel);
    lv_obj_set_size(pass_btns, LV_PCT(100), 50);
    lv_obj_align(pass_btns, LV_ALIGN_TOP_MID, 0, 96);
    lv_obj_set_style_bg_opa(pass_btns, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(pass_btns, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(pass_btns, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(pass_btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pass_btns, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pass_btns, LV_OBJ_FLAG_SCROLLABLE);

    ctx->btn_back = make_btn(pass_btns, LV_SYMBOL_LEFT " Voltar", btn_back_cb, ctx);
    ctx->btn_ok = make_btn(pass_btns, LV_SYMBOL_OK " Confirmar", btn_ok_cb, ctx);
    lv_obj_set_style_bg_color(ctx->btn_ok, COLOR_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lv_obj_get_child(ctx->btn_ok, 0), COLOR_BG, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* PAINEL 3 – Conectando */
    ctx->conn_panel = lv_obj_create(ctx->screen);
    lv_obj_set_size(ctx->conn_panel, LV_PCT(100), LV_PCT(100) - 44);
    lv_obj_align(ctx->conn_panel, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(ctx->conn_panel, COLOR_BG, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ctx->conn_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ctx->conn_panel, 24, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ctx->conn_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ctx->conn_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ctx->conn_panel, LV_OBJ_FLAG_SCROLLABLE);

    ctx->conn_spinner = lv_spinner_create(ctx->conn_panel);
    lv_obj_set_size(ctx->conn_spinner, 80, 80);
    lv_obj_align(ctx->conn_spinner, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_arc_color(ctx->conn_spinner, COLOR_PRIMARY, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ctx->conn_label = lv_label_create(ctx->conn_panel);
    lv_obj_set_width(ctx->conn_label, LV_PCT(100));
    lv_label_set_long_mode(ctx->conn_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(ctx->conn_label, COLOR_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ctx->conn_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ctx->conn_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(ctx->conn_label, LV_ALIGN_TOP_MID, 0, 140);

    ctx->btn_conn_cancel = make_btn(ctx->conn_panel, "Cancelar", btn_conn_cancel_cb, ctx);
    lv_obj_align(ctx->btn_conn_cancel, LV_ALIGN_BOTTOM_MID, 0, -16);
}