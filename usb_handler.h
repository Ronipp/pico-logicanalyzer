#ifndef USB_HANDLER_H
#define USB_HANDLER_H

#include "usb_descriptors.h"

void usb_init();
bool usb_is_configured(void);
void usb_send(end_point *ep, uint8_t *buf, uint8_t len);
uint8_t usb_get(end_point *ep, uint8_t *buf, uint8_t max_len);
void usb_send_ack(void);
void usb_send_config_num(void);
void usb_send_status(void);

void usb_setup_handler(void);
void usb_buff_status_handler(void);
void usb_irq_handler(void);

void usb_reset_bus(void);
void usb_set_address(volatile usb_setup_packet *packet);
void usb_send_string_desc(volatile usb_setup_packet *packet);
void usb_send_dev_desc(volatile usb_setup_packet *packet);
void usb_send_conf_desc(volatile usb_setup_packet *packet);

void usb_set_ep(end_point *ep);

device_descriptor usb_make_dev_desc();
interface_descriptor usb_make_int_desc();
endpoint_descriptor usb_make_end_desc(uint8_t ep_num, bool in);
configuration_descriptor usb_make_conf_desc(uint8_t ep_count, uint8_t int_count);

void ep0_in_func(void);
void ep0_out_func(void);
void ep1_out_func(void);
void ep2_in_func(void);

#endif