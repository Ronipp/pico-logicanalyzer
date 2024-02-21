#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "pinpoller.h"

#define PIO1_DREQ_OFFSET 8

int initial_dma_settings(dma_channel_config *c, bool rx, poller_program prog) {
    int channel = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    uint dreq = (rx) ? DREQ_PIO0_RX0 : DREQ_PIO0_TX0;
    dreq += prog.sm;
    if (prog.pio == pio1) dreq += PIO1_DREQ_OFFSET;
    channel_config_set_dreq(&cfg, dreq);
    *c = cfg;
    return channel;
}

int init_getter_dma(uint32_t *destination, uint destination_length, poller_program prog) {
    dma_channel_config c;
    int channel = initial_dma_settings(&c ,true, prog);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure(
        channel,
        &c,
        destination,
        &prog.pio->rxf[prog.sm],
        destination_length,
        false);
    return channel;
}

int init_setter_dma(uint32_t *payload, poller_program prog) {
    dma_channel_config c;
    int channel = initial_dma_settings(&c, false, prog);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, false);
    dma_channel_configure(
        channel,
        &c,
        &prog.pio->txf[prog.sm],
        payload,
        UINT32_MAX,
        true);
    return channel;
}