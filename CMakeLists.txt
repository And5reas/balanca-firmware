cmake_minimum_required(VERSION 3.10)
project(MeuAppLVGL)

option(DEBUG_MODE "Disable optimizations and enable debugging" OFF)

if (DEBUG_MODE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -ggdb")
endif(DEBUG_MODE)

# Adiciona a pasta do LVGL ao processo de build
add_subdirectory(lvgl)

# Diz onde encontrar o arquivo lv_conf.h
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Cria o seu executável
add_executable(lvgl-app
    # Init
    main.c
    lv_linux_disp_init.c

    # Custom
    src/actions.c
    src/components/wifi_item.c
    src/components/select_wifi.c
    src/components/loader_wifi.c
    src/wifi/action_search_wifi.c
    src/wifi/wifi_manager.c
    src/wifi/wifi_scan.c

    # Images
    src/ui/ui_image_balanca_20.c
    src/ui/ui_image_lock_20.c
    src/ui/ui_image_signal_20.c
    src/ui/ui_image_refresh_20.c
    src/ui/ui_image_seta_back_20.c
    src/ui/ui_image_wifi_20.c
    src/ui/ui_image_wifi_28.c

    # Fonts
    src/ui/ui_font_open_sans_14.c
    src/ui/ui_font_open_sans_16.c
    src/ui/ui_font_open_sans_20.c
    src/ui/ui_font_open_sans_bold_16.c
    src/ui/ui_font_open_sans_bold_20.c
    src/ui/ui_font_open_sans_bold_24.c
    src/ui/ui_font_open_sans_semibold_16.c
    src/ui/ui_font_open_sans_semibold_20.c

    # UI
    src/ui/images.c
    src/ui/screens.c
    src/ui/styles.c
    src/ui/ui.c
)

# Linka a biblioteca do LVGL ao seu executável
# target_link_libraries(lvgl-app lvgl::lvgl)
target_link_libraries(lvgl-app PRIVATE lvgl pthread)