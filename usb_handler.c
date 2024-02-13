
#include "pico/stdlib.h"
#include "hardware/resets.h"
#include "hardware/irq.h"
#include "hardware/regs/usb.h"
#include "hardware/structs/usb.h"
#include <string.h>

#include <stdio.h>

#include "usb_descriptors.h"
#include "usb_handler.h"

#define EP_COUNT 2
#define INTERFACE_COUNT 1

#define usb_hw_clear ((usb_hw_t *)hw_clear_alias_untyped(usb_hw))

// global endpoints
static end_point ep0_in = {
    .number = 0,
    .pid = 1,
    .available = true,
    .buffer = &usb_dpram->ep0_buf_a[0], // buffer is fixed for ep0
    .ep_ctrl = NULL, // no ep control for ep0
    .buf_ctrl = &usb_dpram->ep_buf_ctrl[0].in
};
static end_point ep0_out = {
    .number = 0,
    .pid = 1,
    .available = true,
    .buffer = &usb_dpram->ep0_buf_a[0],
    .ep_ctrl = NULL,
    .buf_ctrl = &usb_dpram->ep_buf_ctrl[0].out
};

static end_point ep1_out = {
    .number = 1,
    .pid = 0,
    .available = true,
    .buffer = usb_dpram->epx_data, // start of shared buffer
    .ep_ctrl = &usb_dpram->ep_ctrl[0].out,
    .buf_ctrl = &usb_dpram->ep_buf_ctrl[1].out
};

static end_point ep2_in = {
    .number = 2,
    .pid = 0,
    .buffer = &usb_dpram->epx_data[64], // next 64 bytes in shared buffer
    .ep_ctrl = &usb_dpram->ep_ctrl[1].in,
    .buf_ctrl = &usb_dpram->ep_buf_ctrl[2].in
};

// global address
static uint8_t device_address = 0;
static bool change_address = false;
// global configured flag
static bool configured = false;

void usb_init() {
    device_address = 0;
    change_address = false;
    configured = false;
    // set the ep_control registers for ep1 and ep2
    usb_set_ep(&ep1_out);
    usb_set_ep(&ep2_in);
    // https://github.com/raspberrypi/pico-examples/blob/master/usb/device/dev_lowlevel/dev_lowlevel.c
    // resetting the usb controller
    reset_block(RESETS_RESET_USBCTRL_BITS);
    unreset_block_wait(RESETS_RESET_USBCTRL_BITS);
    // zero out the usb buffer
    memset(usb_dpram, 0, sizeof(*usb_dpram));
    // muxing the controller to the integrated usb port
    usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;
    // forcing vbus detection so the device thinks its plugged in
    usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
    // enables the controller, leaves bit 1 to 0 for device mode
    usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;
    // enable interrupt on ep0 and pull up dp for full speed
    usb_hw->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS | USB_SIE_CTRL_PULLUP_EN_BITS;

    // set the handler and enable irqs
    irq_set_exclusive_handler(USBCTRL_IRQ, usb_irq_handler);
    irq_set_enabled(USBCTRL_IRQ, true);
    // enable interrupts for setup request, bus reset and buff status change
    usb_hw->inte = USB_INTE_SETUP_REQ_BITS | USB_INTE_BUS_RESET_BITS | USB_INTE_BUFF_STATUS_BITS;

}

bool usb_is_configured(void) {
    return configured;
}

void usb_send(end_point *ep, uint8_t *buf, uint8_t len) {
    if (len > 64) assert(0 && "len has to be less than or equal 64");
    // wait till the ep buffer is in our control (buf_ctrl bit 10 is zero)
    // while (!(ep->available)) tight_loop_contents();
    ep->available = false;
    // copy buffer contents to dpram
    memcpy((void *) ep->buffer, (void *) buf, len);
    // set transfer length, buffer full and pid flags in the control register
    *ep->buf_ctrl = len;
    *ep->buf_ctrl |= USB_BUF_CTRL_FULL;
    *ep->buf_ctrl |= ((ep->pid) ? USB_BUF_CTRL_DATA1_PID : USB_BUF_CTRL_DATA0_PID);
    // datasheet recommends 3 nops before setting available flag after setting other things in buffer control
    // i assume the pid flip takes at least 3 cycles
    ep->pid ^= 1u; // flip pid between 0 and 1
    // set available to 1 so controller can take control
    *ep->buf_ctrl |= USB_BUF_CTRL_AVAIL;
}

uint8_t usb_get(end_point *ep, uint8_t *buf, uint8_t max_len) {
    if (max_len > 64) assert(0 && "len has to be less than or equal 64");
    // get the length of the transfer
    uint16_t len = *ep->buf_ctrl & USB_BUF_CTRL_LEN_MASK;
    // copy data from dpram to buffer
    memcpy((void *)buf, (void *)ep->buffer, MIN(len, max_len));
    // TODO maybe check the pid?
    // set available to 1 so controller can take control
    *ep->buf_ctrl |= USB_BUF_CTRL_AVAIL;
    // return the size of the transfer in bytes
    return len;
}

void usb_send_ack(void) {
    usb_send(&ep0_in, NULL, 0);
}

// called when setup request irq is raised
void usb_setup_handler(void) {
    // the setup packet received
    volatile usb_setup_packet *packet = (volatile usb_setup_packet *) &usb_dpram->setup_packet;
    // pid has to be 1 for sending descriptors
    ep0_in.pid = 1;
    // only handling standard requests
    // if direction is in (device->host)
    if (packet->bmRequestType == USB_DIR_IN) {
        if (packet->bRequest == REQUEST_GET_DESCRIPTOR) {
            uint16_t descriptor = packet->wValue >> 8;
            switch (descriptor) {
            case DEVICE_DESCRIPTOR_TYPE:
                usb_send_dev_desc(packet);
                break;
            case CONFIGURATION_DESCRIPTOR_TYPE:
                usb_send_conf_desc(packet);
                break;
            default:
                assert(0 && "unhandled get_descriptor event");
                break;
            }
        } 
    } else if (packet->bmRequestType == USB_DIR_OUT) {
        switch (packet->bRequest) {
        case REQUEST_SET_ADDRESS:
            usb_set_address(packet);
            break;
        case REQUEST_SET_CONFIGURATION:
        printf("set conf\n");
            // only one configuration so just acknowledge
            usb_send_ack();
            configured = true;
            break;
        default:
            break;
        }
    }
}

void usb_buff_status_handler(void) {
    // copy the unhandled buffer flags
    uint32_t unhandled = usb_hw->buf_status;
    if (unhandled & USB_BUFF_STATUS_EP0_IN_BITS) {
        usb_hw_clear->buf_status = USB_BUFF_STATUS_EP0_IN_BITS;
        if (change_address) {
            // if address change was requested we change it here
            usb_hw->dev_addr_ctrl = device_address;
            change_address = false;
        } else {
            usb_get(&ep0_out, NULL, 0);
        }
        ep0_in.available = 1;
    }
    if (unhandled & USB_BUFF_STATUS_EP0_OUT_BITS) {
        usb_hw_clear->buf_status = USB_BUFF_STATUS_EP0_OUT_BITS;
        ep0_out.available = 1;
    }
    if (unhandled & USB_BUFF_STATUS_EP1_OUT_BITS) {
        usb_hw_clear->buf_status = USB_BUFF_STATUS_EP1_OUT_BITS;
        ep1_out.available = 1;
    }
    if (unhandled & USB_BUFF_STATUS_EP2_IN_BITS) {
        usb_hw_clear->buf_status = USB_BUFF_STATUS_EP2_IN_BITS;
        ep2_in.available = 1;
    }
    if (usb_hw->buf_status != 0) assert(0 && "unhandled end point");
}

void usb_irq_handler(void) {
    // copy the interrupt status register
    uint32_t interrupt_flags = usb_hw->ints;

    if (interrupt_flags & USB_INTS_SETUP_REQ_BITS) {
        // got a setup request
        // remember to clear the irq flag
        usb_hw_clear->sie_status = USB_SIE_STATUS_SETUP_REC_BITS;
        usb_setup_handler();
    }
    if (interrupt_flags & USB_INTS_BUFF_STATUS_BITS) {
        // buffer status changed (buffer is in our control now)
        usb_buff_status_handler();
    }
    if (interrupt_flags & USB_INTS_BUS_RESET_BITS) {
        // got a bus reset request
        //clear the bus reset status from sie_status register
        usb_hw_clear->sie_status = USB_SIE_STATUS_BUS_RESET_BITS;
        usb_reset_bus();
    }
}

void usb_reset_bus(void) {
    printf("reset bus\n");
    device_address = 0;
    change_address = false;
    configured = false;
    usb_hw->dev_addr_ctrl = 0;
}

// this is called during enumeration
void usb_set_address(volatile usb_setup_packet *packet) {
    printf("set addr\n");
    // new device address given during enumeration
    device_address = (packet->wValue & 0xff);
    // address needs to be changed after acknowledging 
    change_address = true;
    usb_send_ack();
}

void usb_send_dev_desc(volatile usb_setup_packet *packet) {
    printf("send dev\n");
    device_descriptor desc = usb_make_dev_desc();
    usb_send(&ep0_in, (uint8_t *)&desc, MIN(packet->wLength, sizeof(desc)));
}

// TODO FIX THIS SHIT
void usb_send_conf_desc(volatile usb_setup_packet *packet) {
    printf("send conf\n");
    configuration_descriptor desc = usb_make_conf_desc(EP_COUNT, INTERFACE_COUNT);
    uint8_t tmp_buf[64];
    memcpy((void *)tmp_buf, (void *)&desc, sizeof(desc));
    uint8_t index = sizeof(desc);
    if (packet->wLength >= desc.wTotalLength) {
        interface_descriptor int_desc = usb_make_int_desc();
        endpoint_descriptor end_desc1 = usb_make_end_desc(ep1_out.number, false);
        endpoint_descriptor end_desc2 = usb_make_end_desc(ep2_in.number, true);
        
        memcpy((void *)(tmp_buf+index), (void *)&int_desc, sizeof(int_desc));
        index += sizeof(int_desc);
        memcpy((void *)(tmp_buf+index), (void *)&end_desc1, sizeof(end_desc1));
        index += sizeof(end_desc1);
        memcpy((void *)(tmp_buf+index), (void *)&end_desc2, sizeof(end_desc2));
        index += sizeof(end_desc2);
    }

    usb_send(&ep0_in, tmp_buf, MIN(packet->wLength, index));
}

device_descriptor usb_make_dev_desc() {
    device_descriptor desc;
    desc.bLength = sizeof(device_descriptor); // size of this descriptor
    desc.bDescriptorType = DEVICE_DESCRIPTOR_TYPE; // type device descriptor
    desc.bcdUSB = USB_SPECIFICATION_NUMBER; // usb 1.1
    desc.bDeviceClass = VENDOR_SPECIFIC; // 0xff vendor specific class
    desc.bDeviceSubClass = 0; // no subclass
    desc.bDeviceProtocol = 0; // no protocol
    desc.bMaxPacketSize = 64; // pico sdk says this is the maximum / this is max for bulk and control
    desc.idVendor = RONALDS_VENDOR_ID; // vendor id
    desc.idProduct = RONALDS_PRODUCT_ID; // product id
    desc.bcdDevice = 0; // no release number
    desc.iManufacturer = 0; // no strings
    desc.iProduct = 0; // no strings
    desc.iSerialNumber = 0; // no strings
    desc.bNumConfigurations = 1; // one configuration

    return desc;    
}

interface_descriptor usb_make_int_desc() {
    interface_descriptor desc;
    desc.bLength = sizeof(interface_descriptor); // length
    desc.bDescriptorType = INTERFACE_DESCRIPTOR_TYPE; // type
    desc.bInterfaceNumber = 1; // first and probably only one
    desc.bAlternateSetting = 1; // this is the alternate
    desc.bNumEndpoints = 2; // two endpoints one for receiving and one for transmitting
    desc.bInterfaceClass = VENDOR_SPECIFIC; // vendor specific
    desc.bInterfaceSubClass = 0; // no subclass
    desc.bInterfaceProtocol = 0; // no protocol
    desc.iInterface = 0; // no strings

    return desc;
}

endpoint_descriptor usb_make_end_desc(uint8_t ep_num, bool in) {
    endpoint_descriptor desc;
    desc.bLength = sizeof(endpoint_descriptor); // size
    desc.bDescriptorType = ENDPOINT_DESCRIPTOR_TYPE; // type
    // shift in bit to leftmost position then or with endpoint number
    desc.bEndpointAddress = ((in ? 1 : 0) << 7) | ep_num; // see usb_descriptors.h for details
    desc.bmAttributes = BULK_TRANSFER_TYPE; // transfer type is bulk
    desc.wMaxPacketSize = MAX_PACKET_SIZE; // 64 is the max packet size for bulk and control transfer
    desc.bInterval = 0; // ignored for bulk and control endpoints

    return desc;
}

configuration_descriptor usb_make_conf_desc(uint8_t ep_count, uint8_t int_count) {
    configuration_descriptor desc;
    desc.bLength = sizeof(configuration_descriptor); // length of configuration descriptor
    desc.bDescriptorType = CONFIGURATION_DESCRIPTOR_TYPE; // type of descriptor 
    desc.wTotalLength = ( // total length of the whole descriptor packet without device descriptor
        (ep_count * sizeof(endpoint_descriptor)) // size of endpoint descriptor times number of them
        + (int_count * sizeof(interface_descriptor)) // size of interface descriptor times number of them
        + sizeof(configuration_descriptor));
    desc.bNumInterfaces = int_count; // how many interfaces
    desc.bConfigurationValue = 1; // this is config 1
    desc.iConfiguration = 0; // no strings
    desc.bmAttributes = CONFIG_ATTRIBUTES; // self powered, no remote wakeup
    desc.bMaxPower = MAXPOWER_100MA; // 100 mA

    return desc;
}

void usb_set_ep(end_point *ep) {
    if (!ep->ep_ctrl) return; // ep0 doesnt have ep ctrl

    *ep->ep_ctrl =  EP_CTRL_ENABLE_BITS |
                    EP_CTRL_INTERRUPT_PER_BUFFER | 
                    (BULK_TRANSFER_TYPE << EP_CTRL_BUFFER_TYPE_LSB) |
                    ((uint32_t)ep->buffer ^ (uint32_t)usb_dpram);
} 

// endpoint functions


