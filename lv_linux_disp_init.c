#include <stdio.h>
#include <stdlib.h>

#include "lvgl/lvgl.h"

#include <fcntl.h>
#include <unistd.h>
#include <linux/kd.h>
#include <sys/ioctl.h>

void lv_linux_disp_init() {
    // Abre o console virtual diretamente, não o tty atual
    int fd = open("/dev/tty1", O_RDWR);
    if (fd < 0) return;

    // Coloca o VT em modo gráfico — o kernel para de renderizar nele
    ioctl(fd, KDSETMODE, KD_GRAPHICS);

    close(fd);
    // NÃO fecha o fd do seu dispositivo de touch — são descritores separados

    /* Inicializa display via framebuffer */
    lv_display_t *disp = lv_linux_fbdev_create();

    const char *fb_dev = getenv("LV_LINUX_FBDEV_DEVICE");
    if(fb_dev == NULL) fb_dev = "/dev/fb0";
    lv_linux_fbdev_set_file(disp, fb_dev);

    /* Reduz tearing para UIs com atualizações pontuais (ex: balança) */
    lv_linux_fbdev_set_force_refresh(disp, false);

    /* Inicializa touch via evdev */
    const char *touch_dev = getenv("LV_LINUX_EVDEV_POINTER_DEVICE");
    if(touch_dev == NULL) touch_dev = "/dev/input/touchscreen0";

    lv_indev_t *touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, touch_dev);
    if(touch == NULL) {
        fprintf(stderr, "Aviso: falha ao inicializar touch em %s\n", touch_dev);
    } else {
        lv_indev_set_display(touch, disp);
        fprintf(stderr, "Touch inicializado: %s\n", touch_dev);
    }
}