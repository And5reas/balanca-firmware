# Wi-Fi Scan — Guia de Integração EEZ Studio + LVGL 9.x

## Estrutura de arquivos

```
wifi/
├── wifi_scan.h              ← Tipos e API de scan (camada de dados)
├── wifi_scan.c              ← Parsing do wpa_cli scan_results
├── wifi_manager.h           ← API pública para a UI
├── wifi_manager.c           ← Orquestração + formatação de strings
└── action_search_wifi.c     ← User Action (cola entre manager e LVGL)
```

---

## Arquitetura em camadas

```
┌────────────────────────────────────────┐
│         User Action (LVGL / EEZ)       │  action_search_wifi.c
│   Sabe de objetos LVGL. Não sabe de   │
│   wpa_cli nem de parsing.              │
└───────────────────┬────────────────────┘
                    │ wifi_display_item_t
┌───────────────────▼────────────────────┐
│           wifi_manager.c               │  Orquestra + formata strings
│   Mantém estado do último scan.        │
│   Não conhece LVGL.                    │
└───────────────────┬────────────────────┘
                    │ wifi_scan_result_t
┌───────────────────▼────────────────────┐
│            wifi_scan.c                 │  Fala com wpa_cli
│   Executa comandos shell, faz parse.   │
│   Não conhece nada além de strings.    │
└────────────────────────────────────────┘
```

---

## Passo 1 — Preparar o EEZ Studio

### 1.1 — Criar o User Widget `WifiItem`

No editor do EEZ Studio, crie um User Widget chamado `WifiItem` com:

| Objeto      | Tipo   | Nome              |
|-------------|--------|-------------------|
| Raiz        | Container | (nome do widget) |
| Filho 0     | Label  | `SSID_name`       |
| Filho 1     | Label  | `description_signal` |
| Filho 2     | Image  | `lock`            |

> **A ordem dos filhos importa!**  
> O código acessa os filhos por índice via `lv_obj_get_child(root, N)`.  
> Se o EEZ Studio gerar getters nominais (ex: `objects.wifi_item0_ssid_name`),
> use-os diretamente — é ainda melhor.

### 1.2 — Criar as instâncias no Container `listWifi`

Na página `WifiConfigScreen`, dentro do container `listWifi`:

1. Insira **16 instâncias** do User Widget `WifiItem`
2. Nomeie-as: `wifi_item0`, `wifi_item1`, ..., `wifi_item15`
3. Configure o layout do `listWifi` como **Flex Column** no LVGL:
   - Isso empilha os itens automaticamente

> O número 16 é o valor de `WIFI_MAX_VISIBLE_ITEMS` em `action_search_wifi.c`.  
> Ajuste conforme a sua necessidade.

### 1.3 — Configurar o layout do container

No EEZ Studio, em `listWifi`:
- Layout: `LV_LAYOUT_FLEX`
- Flex flow: `LV_FLEX_FLOW_COLUMN`
- Padding entre itens: conforme design

---

## Passo 2 — Adaptar os getters em `action_search_wifi.c`

O EEZ Studio gera uma struct global `objects` com todos os widgets nomeados.
Abra o arquivo gerado `ui.h` ou `screens.h` e verifique os nomes exatos.

### Cenário A — EEZ gerou nomes na struct `objects`

```c
// Em ui.h (gerado pelo EEZ):
typedef struct {
    lv_obj_t *wifi_item0;
    lv_obj_t *wifi_item1;
    // ...
    lv_obj_t *wifi_item0_ssid_name;   // se o EEZ nomear filhos também
    // ...
} ui_objects_t;
extern ui_objects_t objects;
```

Nesse caso, a função `get_wifi_item_root()` já está correta — apenas
confirme que os nomes batem com o que foi gerado.

### Cenário B — EEZ gerou os filhos do User Widget também

Se o EEZ Studio gerar getters para os objetos internos do User Widget,
substitua as funções `get_label_ssid()`, `get_label_description()` e
`get_image_lock()` para usar esses getters diretos em vez de
`lv_obj_get_child()`. Isso é mais robusto.

```c
static lv_obj_t *get_label_ssid(int i) {
    switch (i) {
        case 0: return objects.wifi_item0_ssid_name;
        case 1: return objects.wifi_item1_ssid_name;
        // ...
    }
}
```

---

## Passo 3 — Adicionar ao sistema de build (Buildroot / Makefile)

Adicione os arquivos ao seu `CMakeLists.txt` ou `Makefile`:

### CMakeLists.txt

```cmake
target_sources(sua_app PRIVATE
    wifi/wifi_scan.c
    wifi/wifi_manager.c
    wifi/action_search_wifi.c
)

target_include_directories(sua_app PRIVATE
    wifi/
)
```

### Makefile (Buildroot estilo)

```makefile
SRC += wifi/wifi_scan.c
SRC += wifi/wifi_manager.c
SRC += wifi/action_search_wifi.c
CFLAGS += -Iwifi/
```

---

## Passo 4 — Registrar a User Action no EEZ Studio

No EEZ Studio, na aba de **Actions**, crie uma action chamada
`search_wifi`. O EEZ irá declarar automaticamente:

```c
// Em actions.h (gerado pelo EEZ):
void action_search_wifi(lv_event_t *e);
```

A implementação em `action_search_wifi.c` já usa essa assinatura.

---

## Passo 5 — Verificar o wpa_supplicant no Buildroot

Certifique-se que no seu Buildroot config:

```
BR2_PACKAGE_WPA_SUPPLICANT=y
BR2_PACKAGE_WPA_SUPPLICANT_CLI=y
```

E que o `wpa_supplicant` está rodando antes da sua aplicação:

```sh
# /etc/init.d/S40network (ou equivalente)
wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant.conf
```

---

## Saída do wpa_cli (referência de parsing)

```
bssid / frequency / signal level / flags / ssid
aa:bb:cc:dd:ee:ff    2437    -55    [WPA2-PSK-CCMP][ESS]    MinhaRede
11:22:33:44:55:66    5180    -70    [WPA3-SAE][ESS]         RedeRapida
ff:ee:dd:cc:bb:aa    2462    -80    [ESS]                   RedeAberta
```

### Mapeamento de flags → segurança

| Flag(s) no wpa_cli         | Enum gerado        | String na UI  |
|----------------------------|--------------------|---------------|
| `[SAE]`                    | `WIFI_SEC_WPA3`    | `WPA3`        |
| `[WPA2-PSK-...]`           | `WIFI_SEC_WPA2`    | `WPA2`        |
| `[WPA-PSK-...]`            | `WIFI_SEC_WPA`     | `WPA`         |
| `[WEP]`                    | `WIFI_SEC_WEP`     | `WEP`         |
| `[EAP]`                    | `WIFI_SEC_ENTERPRISE` | `EAP`      |
| `[ESS]` (sem WPA/WEP)      | `WIFI_SEC_OPEN`    | `Aberta`      |

---

## Comportamento em runtime

Quando o usuário pressiona o botão de scan:

```
action_search_wifi()
  │
  ├─ wifi_item_hide(0..15)       ← limpa a lista
  │
  ├─ wifi_manager_scan()
  │     ├─ wpa_cli scan          ← dispara o scan
  │     ├─ sleep(3)              ← aguarda
  │     └─ wpa_cli scan_results  ← lê e parseia
  │
  └─ Para cada rede (até 16):
        └─ wifi_item_populate(i)
              ├─ lv_label_set_text(ssid, "MinhaRede")
              ├─ lv_label_set_text(desc, "2.4 GHz • WPA2")
              └─ lock: remove/add LV_OBJ_FLAG_HIDDEN
```

---

## Nota sobre o scan bloqueante

A implementação atual bloqueia a task do LVGL por ~3 segundos durante o scan.

Para produção, considere:

1. **Thread separada** (pthreads): rodar o scan em background e usar
   `lv_async_call()` para atualizar a UI quando pronto.

2. **lv_timer**: disparar o scan com `popen()` assíncrono e verificar
   periodicamente com um timer LVGL de 500ms.

A versão bloqueante é funcional para prototipagem e dispositivos simples.
