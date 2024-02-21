#include "hardware/pio.h"
#include "pico/stdlib.h"
#include "pinpoller.pio.h"
#include "stdio.h"
#include "pinpoller.h"


void pinpoller_program_init(poller_program prog) {
    pio_sm_set_enabled(prog.pio, prog.sm, false);
    uint offset = pio_add_program(prog.pio, &pinpoller_program);
    pio_sm_config c = pinpoller_program_get_default_config(offset);
    sm_config_set_jmp_pin(&c, prog.pin);
    sm_config_set_clkdiv_int_frac(&c, prog.poll_rate, 0);
    sm_config_set_in_shift(&c, true, true, 32); // autopush enable shift right
    sm_config_set_out_shift(&c, true, true, 32); // autopull enable shift right

    pio_sm_init(prog.pio, prog.sm, offset + pinpoller_offset_start, &c);
}

void pinpoller_clear_fifo(poller_program prog) {
    pio_sm_clear_fifos(prog.pio, prog.sm);
    
}
