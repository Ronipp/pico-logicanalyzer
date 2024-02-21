#pragma once

#include "hardware/pio.h"
#include "hardware/pio.h"

typedef enum {
    SR_125MHZ = 1,
    SR_62MHZ = 2,
    SR_31MHZ = 3,
} sample_rates;

typedef struct {
    uint pin;               // pin to poll
    PIO pio;                // pio to use
    uint sm;                // statemachine to use
    sample_rates poll_rate; // poll rate to use
} poller_program;


void pinpoller_program_init(poller_program prog);

