#include "pico/stdio.h"
#include "usb_handler.h"
#include <stdio.h>


int main() {
    stdio_init_all();
    usb_init();

    while (!usb_is_configured()) tight_loop_contents();
    printf("success\n");
    while (1) tight_loop_contents();
}