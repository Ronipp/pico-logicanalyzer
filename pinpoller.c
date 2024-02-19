#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "pinpoller.pio.h"
#include "stdio.h"
#include "pinpoller.h"

static uint sm;
static PIO pio;


void pinpoller_program_init(uint pinbase, int number_of_pins, sample_rates polling_rate, uint statemachine, PIO pio_inst) {
    pio_sm_set_enabled(pio_inst, statemachine, false);
    uint16_t pinpoller_program_instructions[] = {pio_encode_in(pio_pins, number_of_pins)};
    struct pio_program pinpoller_program = {
        .instructions = pinpoller_program_instructions,
        .length = 1,
        .origin = -1,
    };
    uint offset = pio_add_program(pio_inst, &pinpoller_program);
    sm = statemachine;
    pio = pio_inst;
    for (int i=0; i<number_of_pins; i++) {
        pio_gpio_init(pio, pinbase+i);

    }
    pio_sm_set_consecutive_pindirs(pio_inst,sm, pinbase, number_of_pins, false);

    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset);
    sm_config_set_in_pins(&c, pinbase);
    sm_config_set_in_shift(&c, false, true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    sm_config_set_clkdiv_int_frac(&c, polling_rate, 0);

    pio_sm_init(pio_inst, sm, offset, &c);

}

void pinpoller_compress(uint32_t data) {
    uint32_t compressed = 0;
    for (int i=0; i<32; i++) {
        
    }
}
