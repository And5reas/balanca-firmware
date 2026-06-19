#include "botao_incrementa.h"
#include <stdio.h>

/* * O SEGREDINHO DO ENCAPSULAMENTO (A palavra 'static'):
 * Colocar 'static' na variável e na função de callback significa que elas 
 * SÓ EXISTEM dentro deste arquivo. O main.c nunca poderá acessá-las 
 * diretamente, evitando bugs e conflitos de variáveis com o mesmo nome.
 */
static int itens_pesados = 0;

static void btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * label_contador = lv_event_get_user_data(e);

    if(code == LV_EVENT_CLICKED) {
        itens_pesados++;
        lv_label_set_text_fmt(label_contador, "Itens na balanca: %d", itens_pesados);
    }
}

/* A implementação da nossa função pública */
void criar_botao_incrementa(lv_obj_t * parent) {
    /* Cria o Label do Contador preso ao 'parent' (que será a nossa tela) */
    lv_obj_t * label_contador = lv_label_create(parent);
    lv_label_set_text(label_contador, "Itens na balanca: 0");
    lv_obj_align(label_contador, LV_ALIGN_TOP_MID, 0, 50);

    /* Cria o Botão preso ao 'parent' */
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_size(btn, 140, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);

    /* Cria o texto do botão */
    lv_obj_t * label_btn = lv_label_create(btn);
    lv_label_set_text(label_btn, "Adicionarrrr");
    lv_obj_center(label_btn);

    /* Adiciona o evento, passando o label_contador como referência */
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, label_contador);
}