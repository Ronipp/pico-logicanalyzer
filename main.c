#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pinpoller.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "dma_handler.h"
#include "usb_handler.h"
#include <stdio.h>
#include <string.h>

#define PIN 28

#define TX_PAYLOAD 0xFEFEFEFE


static bool start = false;
void ep1_func(uint8_t *buffer, uint8_t *len) {
    if (strcmp(buffer, "start") == 0) start = true;
}

int main() {
    stdio_init_all();
    usb_init();

    poller_program prog = {PIN, pio0, pio_claim_unused_sm(pio0, true), SR_125MHZ};
    pinpoller_program_init(prog);
    
    uint32_t buf[1024] = {0};
    
    int channel_rx = init_getter_dma(buf, 1024, prog);
    uint32_t payload = TX_PAYLOAD;
    int channel_tx = init_setter_dma(&payload, prog);

    while (!start) tight_loop_contents();
    pinpoller_clear_fifo(prog);
    dma_channel_start(channel_rx);
    pio_sm_set_enabled(prog.pio, prog.sm, true);

    dma_channel_wait_for_finish_blocking(channel_rx);
    pio_sm_set_enabled(prog.pio, prog.sm, false);


    
    while (1) {
        printf("aa");
    }
}