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
    main.c
    lv_linux_disp_init.c
    # src/components/wifi_screen.c

    src/ui/ui_image_balanca.c
    src/ui/ui_font_open_sans.c
    src/ui/images.c
    src/ui/screens.c
    src/ui/styles.c
    src/ui/ui.c
    src/actions.c
)

# Linka a biblioteca do LVGL ao seu executável
# target_link_libraries(lvgl-app lvgl::lvgl)
target_link_libraries(lvgl-app PRIVATE lvgl)