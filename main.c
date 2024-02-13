#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pinpoller.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include <stdio.h>

#define PIN 28

int init_dma(uint32_t *destination, size_t size, PIO pio, uint sm) {
    int channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_dreq(&c, DREQ_PIO0_RX0);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure(
        channel,
        &c,
        destination,
        &pio->rxf[sm],
        size,
        false);
    return channel;
}

int main() {
    stdio_init_all();

    uint32_t buf[1024] = {0};
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    pinpoller_program_init(PIN, 1, SR_125MHZ, sm, pio);
    
    int channel = init_dma(buf, 1024, pio, sm);
    dma_channel_start(channel);
    pio_sm_set_enabled(pio, sm, true);

    dma_channel_wait_for_finish_blocking(channel);
    pio_sm_set_enabled(pio, sm, false);


    
    while (1) {
        printf("aa");
    }
}