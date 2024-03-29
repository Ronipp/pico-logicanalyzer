#include "pico/stdio.h"
#include "usb_handler.h"
#include <stdio.h>

void ep1_func(uint8_t *buffer, uint8_t *len) {
    printf("%s\n", buffer);
}

void ep2_func(end_point *ep, uint8_t should_handle) {
    printf("yes %d\n", should_handle);
}

int main() {
    stdio_init_all();
    usb_init();

    while (!usb_is_configured()) tight_loop_contents();
    printf("success\n");

    usb_register_ep1_out_func(ep1_func);
    usb_register_ep2_in_func(ep2_func);

    uint8_t buf0[64] = {1};
    uint8_t buf1[64] = {2};
    usb_ep2_send(0, buf0, 64);
    usb_ep2_send(1, buf1, 64);


    while (1) tight_loop_contents();
}