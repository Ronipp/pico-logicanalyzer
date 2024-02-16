#include "pico/stdio.h"
#include "usb_handler.h"
#include <stdio.h>

void ep1_func(uint8_t *buffer, uint8_t len) {
    printf("%s", buffer);
}

int main() {
    stdio_init_all();
    usb_init();

    while (!usb_is_configured()) tight_loop_contents();
    printf("success\n");

    usb_register_ep1_out_func(ep1_func);

    while (1) tight_loop_contents();
}