#pragma once

#include "hardware/pio.h"

typedef enum {
    SR_125MHZ = 1,
    SR_62MHZ = 2,
    SR_31MHZ = 3,
} sample_rates;

void pinpoller_program_init(uint pinbase, int number_of_pins, sample_rates polling_rate, uint statemachine, PIO pio_inst);

