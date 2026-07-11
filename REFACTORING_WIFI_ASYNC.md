# Refatoração de Wi-Fi Não-Bloqueante - Resumo das Alterações

## 📋 Objetivo
Refatorar o fluxo de busca de Wi-Fi para eliminar o travamento da UI durante o scan, implementando arquitetura com threading e callbacks.

---

## ✅ Alterações Realizadas

### 1. **Novo arquivo: `wifi_thread.h`**
- Define a estrutura `wifi_scan_result_ui_t` para comunicação entre threads
- Declara `wifi_scan_thread()` - função executada na thread secundária
- Declara `update_wifi_ui()` - callback executado na thread principal do LVGL
- Zero dependência com LVGL (apenas estruturas de dados)

### 2. **Refatoração de `action_search_wifi.c`**

#### Mudanças principais:
- **Adicionado**: `#include <pthread.h>` e `#include "wifi_thread.h"`
- **action_search_wifi()** - Agora APENAS cria a thread:
  - `pthread_create(&scan_thread_id, NULL, wifi_scan_thread, NULL)`
  - `pthread_detach(scan_thread_id)` - Correto uso: passa `pthread_t`, não ponteiro
  - Retorna imediatamente (UI nunca congela)

- **Novo**: `wifi_scan_thread(void *arg)` - Thread secundária:
  - Aloca `wifi_scan_result_ui_t` no heap
  - Executa `wifi_manager_scan()` (bloqueante ~3s) sem afetar UI
  - Coleta resultados formatados de `wifi_manager_get_item()`
  - Chama `lv_async_call(update_wifi_ui, resultado)` para sincronizar

- **Novo**: `update_wifi_ui(void *user_data)` - Callback LVGL:
  - Executa SEMPRE na thread principal
  - Atualiza widgets (`lv_label_set_text()`, `lv_obj_add_flag()`, etc.)
  - Popula a lista de redes
  - Libera memória do resultado

#### Mantido:
- Funções de acesso aos widgets: `get_wifi_item_root()`, `get_label_ssid()`, etc.
- Helper `wifi_item_populate()` - Agora chamada APENAS de `update_wifi_ui()`
- Helper `wifi_item_hide()` - Agora chamada APENAS de `update_wifi_ui()`

### 3. **CMakeLists.txt**
- Adicionado `pthread` na linkagem:
  ```cmake
  target_link_libraries(lvgl-app PRIVATE lvgl pthread)
  ```

---

## 🏗️ Arquitetura Resultante

```
┌─────────────────────────────┐
│  action_search_wifi()       │  ◄─── User pressiona botão (Thread LVGL)
│  (Thread Principal LVGL)    │
│  ├── pthread_create()       │  ◄─── Cria thread secundária
│  ├── pthread_detach()       │  ◄─── Desacopla
│  └── return imediatamente   │  ◄─── UI continua responsiva
└─────────────────────────────┘
         │
         │  (Não bloqueia aqui)
         ▼
┌─────────────────────────────┐
│  wifi_scan_thread()         │  ◄─── Thread Secundária
│  (Thread de Background)     │
│  ├── malloc()               │  ◄─── Aloca resultado
│  ├── wifi_manager_scan()    │  ◄─── BLOQUEANTE ~3s (OK aqui!)
│  │   ├── wifi_scan_run()    │
│  │   ├── system("wpa_cli")  │
│  │   ├── sleep(3)           │
│  │   └── popen() parser      │
│  ├── wifi_manager_get_item()│  ◄─── Copia resultados
│  └── lv_async_call()        │  ◄─── Agenda callback
└─────────────────────────────┘
         │
         │  (Thread-safe)
         ▼
┌─────────────────────────────┐
│  update_wifi_ui()           │  ◄─── Callback LVGL
│  (Thread Principal LVGL)    │
│  ├── lv_label_set_text()    │  ◄─── Atualiza widgets
│  ├── lv_obj_add_flag()      │
│  ├── lv_obj_remove_flag()   │
│  ├── wifi_item_populate()   │  ◄─── Popula lista
│  ├── free()                 │  ◄─── Libera memória
│  └── return                 │
└─────────────────────────────┘
```

---

## 🔒 Segurança de Threading

### Sem Race Conditions:
1. **Variáveis de UI** (`lv_obj_t*`) - NUNCA compartilhadas entre threads
2. **Dados de resultado** - Copiados para buffer temporário em `wifi_scan_thread()`
3. **Sincronização** - Via `lv_async_call()` (garantida thread-safe pelo LVGL)
4. **Sem mutex/semáforo** - Desnecessário; comunicação unidirecional thread→UI

### Correctness:
- Nenhuma função do LVGL chamada fora da thread principal
- Única exceção permitida: `lv_async_call()` (thread-safe por design)

---

## 📝 Requisitos Atendidos

✅ 1. `action_search_wifi()` apenas cria thread  
✅ 2. `wifi_scan_thread()` implementada corretamente  
✅ 3. `update_wifi_ui()` com todas as chamadas LVGL  
✅ 4. `wifi_manager_scan()` continua puro (só lógica de Wi-Fi)  
✅ 5. Separação clara: thread de Wi-Fi vs. thread LVGL  
✅ 6. Sem operações bloqueantes na thread principal  
✅ 7. Apenas `lv_async_call()` chamada entre threads  
✅ 8. Estruturas de dados bem organizadas (`wifi_scan_result_ui_t`)  

---

## 🚀 Comportamento Esperado

### Antes (COM travamento):
```
1. Usuário clica "Buscar Wi-Fi"
2. action_search_wifi() chamada
3. wifi_manager_scan() bloqueante por ~3s
4. ❌ UI CONGELA completamente
5. Usuário vê tela travada
6. Após ~3s, lista de redes aparece
```

### Depois (SEM travamento):
```
1. Usuário clica "Buscar Wi-Fi"
2. action_search_wifi() cria thread e retorna imediatamente
3. ✅ UI continua 100% responsiva
4. Usuário pode interagir normalmente
5. Thread secundária faz wi-Fi manager_scan() bloqueante (~3s)
6. Thread chama lv_async_call() → agenda callback
7. Logo após, update_wifi_ui() executa na thread principal
8. Lista de redes aparece automaticamente
9. UI atualiza suavemente
```

---

## 🔧 Como Compilar

```bash
cd /home/and5reas/Projetos/versao_1/lv_buildroot/application/balanca
mkdir -p build
cd build
cmake ..
cmake --build . --parallel
```

---

## 📦 Arquivos Modificados

| Arquivo | Tipo | Alteração |
|---------|------|-----------|
| `wifi_thread.h` | Novo | Header com estruturas de threading |
| `action_search_wifi.c` | Modificado | Refatoração com threading completo |
| `wifi_manager.c` | Não alterado | Mantém lógica pura de Wi-Fi |
| `wifi_scan.c` | Não alterado | Continua executando em thread secundária |
| `CMakeLists.txt` | Modificado | Adicionado link com `pthread` |

---

## 🎯 Próximos Passos Opcionais

### 1. Cancelamento de Scan em Background
```c
volatile sig_atomic_t g_scan_cancel = false;

// Em wifi_scan_thread():
if (g_scan_cancel) { pthread_exit(NULL); }

// Ação "Cancelar":
void action_cancel_wifi(lv_event_t *e) {
    g_scan_cancel = true;
}
```

### 2. Indicador Visual de Progresso
```c
// Em update_wifi_ui():
lv_spinner_create(container);  // Spinner enquanto aguarda
```

### 3. Tratamento de Erros
```c
// Em update_wifi_ui():
if (!result->success) {
    lv_label_set_text(error_label, result->error_msg);
}
```

### 4. Atualização Periódica de RSSI
```c
static void on_rssi_timer(lv_timer_t *t) {
    // Reread RSSI sem novo scan completo
}
```

### 5. Integração com wpa_supplicant D-Bus
- Mais eficiente que `system("wpa_cli")`
- Notificações em tempo real de eventos de scan

---

## ✨ Boas Práticas Aplicadas

1. ✅ **Separação de responsabilidades**
   - Thread de Wi-Fi: dados e operações bloqueantes
   - Thread LVGL: interface e widgets

2. ✅ **Zero acoplamento LVGL-WiFi**
   - `wifi_manager.c`, `wifi_scan.c` não conhecem LVGL
   - Fácil de reutilizar em outras plataformas

3. ✅ **Callback-driven architecture**
   - Assíncrono e responsivo
   - Padrão comum em aplicações modernas

4. ✅ **Segurança de memória**
   - Alocação/liberação clara em `malloc()`/`free()`
   - Sem vazamentos ou double-free

5. ✅ **Tratamento de erros**
   - `pthread_create()` e `pthread_detach()` verificam retorno
   - Mensagens no stderr para debug

6. ✅ **Documentação inline**
   - Cada função tem propósito claro
   - Comentários explicam decisões de design

---

## 📚 Referências

- **POSIX Threads**: `man pthread_create`, `man pthread_detach`
- **LVGL Async**: `lv_async_call()` - Thread-safe callback scheduling
- **Raspberry Pi Zero 2 W**: ARM Cortex-A53, suporta pthreads nativamente
- **wpa_supplicant**: Integração com `wpa_cli` para scans Wi-Fi

---

**Status**: ✅ Refatoração Completa | Pronto para Compilação e Testes
