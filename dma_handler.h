#pragma once

int init_getter_dma(uint32_t *destination, uint destination_length, poller_program prog);
int init_setter_dma(uint32_t *payload, poller_program prog);