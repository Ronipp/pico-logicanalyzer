
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

#define MANUFACTURER_STRING_INDEX 1
#define PRODUCT_STRING_INDEX 2
#define WINDOWS_STRING_DESCRIPTOR_INDEX 0xee

#define MS_OS_VENDOR_ID 0x42
#define MS_BCD_VER 0x0100

#define usb_hw_clear ((usb_hw_t *)hw_clear_alias_untyped(usb_hw))

// global endpoints
static end_point ep0_in = {
    .number = 0,
    .pid = 1,
    .buffer = &usb_dpram->ep0_buf_a[0], // buffer is fixed for ep0
    .buffer_second = NULL,
    .ep_ctrl = NULL, // no ep control for ep0
    .buf_ctrl = (buf_ctrl_struct *)&usb_dpram->ep_buf_ctrl[0].in,
};
static end_point ep0_out = {
    .number = 0,
    .pid = 1,
    .buffer = &usb_dpram->ep0_buf_a[0],
    .buffer_second = NULL,
    .ep_ctrl = NULL,
    .buf_ctrl = (buf_ctrl_struct *)&usb_dpram->ep_buf_ctrl[0].out,
};

static end_point ep1_out = {
    .number = 1,
    .pid = 0,
    .buffer = &usb_dpram->epx_data[0], // start of shared buffer
    .buffer_second = NULL,
    .ep_ctrl = &usb_dpram->ep_ctrl[0].out,
    .buf_ctrl = (buf_ctrl_struct *)&usb_dpram->ep_buf_ctrl[1].out,
};

static end_point ep2_in = {
    .number = 2,
    .pid = 0,
    .buffer = &usb_dpram->epx_data[64], // next 64 bytes in shared buffer
    .buffer_second = &usb_dpram->epx_data[64 * 2], // the next 64 bytes after first buffer
    .ep_ctrl = &usb_dpram->ep_ctrl[1].in,
    .buf_ctrl = (buf_ctrl_struct *)&usb_dpram->ep_buf_ctrl[2].in,
};

// global address
static uint8_t device_address = 0;
static bool change_address = false;
// global configured flag
static bool configured = false;
// global end point functions
static ep_func_ptr user_ep1_func = NULL;
static ep2_func_ptr user_ep2_func = NULL;

void usb_init() {
    device_address = 0;
    change_address = false;
    configured = false;
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
    usb_hw->inte = USB_INTE_SETUP_REQ_BITS | USB_INTE_BUS_RESET_BITS | USB_INTE_BUFF_STATUS_BITS | USB_INTE_ERROR_DATA_SEQ_BITS;

    // set the ep_control registers for ep1 and ep2
    usb_set_ep(&ep1_out);
    usb_set_ep(&ep2_in);
    usb_set_ep_available(&ep1_out);
    usb_set_ep_double_buffered(&ep2_in);

}

bool usb_is_configured(void) {
    return configured;
}

void usb_send(end_point *ep, uint8_t buf_num, uint8_t *buf, uint8_t len) {
    if (len > 64) assert(0 && "len has to be less than or equal 64");
    // copy buffer contents to dpram
    volatile uint8_t *ep_buf = ep->buffer;
    if (buf_num == 1) ep_buf = ep->buffer_second;
    memcpy((void *) ep_buf, (void *) buf, len);
    // set transfer length, buffer full and pid flags in the control register
    uint32_t buf_ctrl = len | USB_BUF_CTRL_FULL;
    if (ep->pid == 1) buf_ctrl |= USB_BUF_CTRL_DATA1_PID;
    if (buf_num == 1) {
        ep->buf_ctrl->second = buf_ctrl;
    } else {
        ep->buf_ctrl->first = buf_ctrl; // set buffer control bits
    }
    // datasheet recommends 3 nops before setting available flag after setting other things in buffer control
    // i assume the pid flip takes at least 3 cycles
    ep->pid ^= 1u; // flip pid between 0 and 1
    // set available to 1 so controller can take control
    if (buf_num == 1) {
        ep->buf_ctrl->second |= USB_BUF_CTRL_AVAIL;    
    } else {
        ep->buf_ctrl->first |= USB_BUF_CTRL_AVAIL;
    }
}

void usb_ep2_send(uint8_t buf_num, uint8_t *buf, uint8_t len) {
    usb_send(&ep2_in, buf_num, buf, len);
}


uint8_t usb_get(end_point *ep, uint8_t *buf, uint8_t max_len) {
    if (max_len > 64) assert(0 && "len has to be less than or equal 64");
    // get the length of the transfer
    uint16_t len = ep->buf_ctrl->first & USB_BUF_CTRL_LEN_MASK;
    // copy data from dpram to buffer
    memcpy((void *)buf, (void *)ep->buffer, MIN(len, max_len));
    // set buf ctrl full bit to zero
    ep->buf_ctrl->first &= ~USB_BUF_CTRL_FULL;
    ep->pid ^= 1u; // flip pid between 0 and 1
    if (ep->pid == 1) {
        ep->buf_ctrl->first |= USB_BUF_CTRL_DATA1_PID;
    } else {
        ep->buf_ctrl->first &= ~USB_BUF_CTRL_DATA1_PID;
    }
    // for some reason the buf ctrl length has to be set to max in order to receive data
    ep->buf_ctrl->first |= 64;
    // set available to 1 so controller can take control
    ep->buf_ctrl->first |= USB_BUF_CTRL_AVAIL;
    // return the size of the transfer in bytes
    return len;
}

void usb_send_ack(void) {
    usb_send(&ep0_in, 0, NULL, 0);
}

void usb_send_stall(void) {
    usb_hw->ep_stall_arm = 0x1;
    ep0_in.buf_ctrl->first |= USB_BUF_CTRL_STALL;
}

void usb_send_config_num(void) {
    uint8_t config_num = 1;
    usb_send(&ep0_in, 0, &config_num, 1);
}

void usb_send_status(void) {
    printf("send status\n");
    uint16_t status = DEVICE_STATUS;
    usb_send(&ep0_in, 0, (uint8_t *)&status, 2);
}

// called when setup request irq is raised
void usb_setup_handler(void) {
    // the setup packet received
    volatile usb_setup_packet *packet = (volatile usb_setup_packet *) &usb_dpram->setup_packet;
    // pid has to be 1 for sending descriptors
    ep0_in.pid = 1;
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
            case STRING_DESCRIPTOR_TYPE:
                usb_send_string_desc(packet);
                break;
            case QUALIFIER_DESCRIPTOR_TYPE:
                // no high speed unfortunately
                usb_send_stall();
                break;
            default:
                assert(0 && "unhandled get_descriptor event");
                break;
            }
        } else if (packet->bRequest == REQUEST_GET_CONFIGURATION) {
            usb_send_config_num();
        } else if (packet->bRequest == REQUEST_GET_STATUS) {
            usb_send_status();
        } else {
            assert(0 && "some other in request");
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
            assert(0 && "some other out request");
            break;
        }
    } else if (packet->bmRequestType == MS_REQUEST_TYPE) {
        assert(packet->bRequest == MS_OS_VENDOR_ID);
        usb_send_winusb_desc(packet);
    } else if (packet->bmRequestType == MS_EXT_PROP_REQUEST) {
        usb_send_ms_props_desc(packet);
    } else {
        assert(0 && "some other request");
    }
}

void usb_buff_status_handler(void) {
    // copy the unhandled buffer flags
    uint32_t unhandled = usb_hw->buf_status;
    if (unhandled & USB_BUFF_STATUS_EP0_IN_BITS) {
        usb_hw_clear->buf_status = USB_BUFF_STATUS_EP0_IN_BITS;
        ep0_in_func();
    }
    if (unhandled & USB_BUFF_STATUS_EP0_OUT_BITS) {
        usb_hw_clear->buf_status = USB_BUFF_STATUS_EP0_OUT_BITS;
        ep0_out_func();
    }
    if (unhandled & USB_BUFF_STATUS_EP1_OUT_BITS) {
        usb_hw_clear->buf_status = USB_BUFF_STATUS_EP1_OUT_BITS;
        ep1_out_func();
    }
    if (unhandled & USB_BUFF_STATUS_EP2_IN_BITS) {
        usb_hw_clear->buf_status = USB_BUFF_STATUS_EP2_IN_BITS;
        uint8_t should_handle = (uint8_t)(usb_hw->buf_cpu_should_handle >> 4);
        user_ep2_func(&ep2_in, should_handle);
        
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
    } if (interrupt_flags & USB_INTS_ERROR_DATA_SEQ_BITS) {
        usb_hw_clear->sie_status = USB_SIE_STATUS_DATA_SEQ_ERROR_BITS;
        printf("data sequence error\n");
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
    // new device address given during enumeration
    device_address = (packet->wValue & 0xff);
    printf("set addr: %d\n", device_address);
    // address needs to be changed after acknowledging 
    change_address = true;
    usb_send_ack();
}

void usb_send_string_desc(volatile usb_setup_packet *packet) {
    printf("send string\n");
    uint8_t index = packet->wValue & 0xff;
    if (index == 0) {
        language_descriptor desc = {
            .bLength = sizeof(language_descriptor),
            .bDescriptorType = STRING_DESCRIPTOR_TYPE,
            .wLANGID0 = LANG_US
        };
        usb_send(&ep0_in, 0, (uint8_t *)&desc, MIN(sizeof(language_descriptor), packet->wLength));
    } else if (index == MANUFACTURER_STRING_INDEX) {
        const char *string = "Ronald";
        uint8_t buflen = ((2 * strlen(string)) + sizeof(string_descriptor_head));
        string_descriptor_head head = {
            .bLength = buflen,
            .bDescriptorType = STRING_DESCRIPTOR_TYPE
        };
        uint8_t buf[64];
        memcpy((void *)buf, (void *)&head, sizeof(head));
        usb_make_str_to_unicode(string, buf + sizeof(head), 64 - sizeof(head));
        usb_send(&ep0_in, 0, buf, MIN(buflen, packet->wLength));
    } else if (index == PRODUCT_STRING_INDEX) {
        const char *string = "Logic";
        uint8_t buflen = ((2 * strlen(string)) + sizeof(string_descriptor_head));
        string_descriptor_head head = {
            .bLength = buflen,
            .bDescriptorType = STRING_DESCRIPTOR_TYPE
        };
        uint8_t buf[64];
        memcpy((void *)buf, (void *)&head, sizeof(head));
        usb_make_str_to_unicode(string, buf + sizeof(head), 64 - sizeof(head));
        usb_send(&ep0_in, 0, buf, MIN(buflen, packet->wLength));
    } else if (index == WINDOWS_STRING_DESCRIPTOR_INDEX) {
        ms_os_string_descriptor desc = usb_make_ms_os_str_desc();
        usb_send(&ep0_in, 0, (uint8_t *)&desc, MIN(sizeof(desc), packet->wLength));
    } else {
        printf("%d : %d\n", packet->wValue, packet->wIndex);
        assert(0 && "some other index");
    }
}

void usb_make_str_to_unicode(const char *str, uint8_t *dst, uint8_t len) {
    if (len < (2*strlen(str))) assert(0 && "destination buffer too short");
    for (int i=0; i<strlen(str); i++) {
        dst[(2*i)] = str[i];
        dst[(2*i) + 1] = 0;
    }
}

ms_os_string_descriptor usb_make_ms_os_str_desc(void) {
    ms_os_string_descriptor desc;
    desc.bLength = sizeof(ms_os_string_descriptor);
    desc.bDescriptorType = STRING_DESCRIPTOR_TYPE;
    const char *str = "MSFT100";
    usb_make_str_to_unicode(str, desc.qwSignature, 14);
    desc.bMS_VendorCode = MS_OS_VENDOR_ID;
    desc.bPad = 0;
    return desc;
}

void usb_send_winusb_desc(volatile usb_setup_packet *packet) {
    printf("send winusb\n");
    winsub_descriptor desc = usb_make_winusb_desc();
    usb_send(&ep0_in, 0, (uint8_t *)&desc, MIN(sizeof(desc), packet->wLength));
}

void usb_send_ms_props_desc(volatile usb_setup_packet *packet) {
    printf("send ms props\n");
    ms_extended_properties_descriptor desc = usb_make_ms_props_desc();
    usb_send(&ep0_in, 0, (uint8_t *)&desc, MIN(sizeof(desc), packet->wLength));
}

void usb_send_dev_desc(volatile usb_setup_packet *packet) {
    printf("send dev\n");
    device_descriptor desc = usb_make_dev_desc();
    usb_send(&ep0_in, 0, (uint8_t *)&desc, MIN(packet->wLength, sizeof(desc)));
}

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

    usb_send(&ep0_in, 0, tmp_buf, MIN(packet->wLength, index));
}

device_descriptor usb_make_dev_desc() {
    device_descriptor desc;
    desc.bLength = sizeof(device_descriptor); // size of this descriptor
    desc.bDescriptorType = DEVICE_DESCRIPTOR_TYPE; // type device descriptor
    desc.bcdUSB = USB_SPECIFICATION_NUMBER; // usb 2
    desc.bDeviceClass = 0; // specified in interface descriptor
    desc.bDeviceSubClass = 0; // no subclass
    desc.bDeviceProtocol = 0; // no protocol
    desc.bMaxPacketSize = 64; // pico sdk says this is the maximum / this is max for bulk and control
    desc.idVendor = RONALDS_VENDOR_ID; // vendor id
    desc.idProduct = RONALDS_PRODUCT_ID; // product id
    desc.bcdDevice = 0; // no release number
    desc.iManufacturer = MANUFACTURER_STRING_INDEX; // index of string
    desc.iProduct = PRODUCT_STRING_INDEX; // index of string
    desc.iSerialNumber = 0; // no strings
    desc.bNumConfigurations = 1; // one configuration

    return desc;    
}

interface_descriptor usb_make_int_desc() {
    interface_descriptor desc;
    desc.bLength = sizeof(interface_descriptor); // length
    desc.bDescriptorType = INTERFACE_DESCRIPTOR_TYPE; // type
    desc.bInterfaceNumber = 0; // first and probably only one
    desc.bAlternateSetting = 0; // this is the alternate
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

winsub_descriptor usb_make_winusb_desc(void) {
    winsub_descriptor desc = {
        .dwLength = sizeof(winsub_descriptor),
        .bcdVersion = MS_BCD_VER, // version one
        .wIndex = MS_OS_INDEX, // from documentation
        .bCount = 0x1, // one thing
        .reserved = {0},
        .bFirstInterfaceNumber = 0,
        .reserve = 0x01, // from documentation
        .compatibleID = {'W', 'I', 'N', 'U', 'S', 'B', 0, 0},
        .subCompatibleID = {0},
        .reserv = {0}
    };
    return desc;
}

ms_extended_properties_descriptor usb_make_ms_props_desc(void) {
    ms_extended_properties_descriptor desc = {
        .dwLength = sizeof(ms_extended_properties_descriptor),
        .bcdVersion = MS_BCD_VER,
        .wIndex = 0x5, // ?
        .wCount = 0 // no extended properties
    };
}

void usb_set_ep(end_point *ep) {
    if (!ep->ep_ctrl) return; // ep0 doesnt have ep ctrl

    *ep->ep_ctrl =  EP_CTRL_ENABLE_BITS |
                    EP_CTRL_INTERRUPT_PER_BUFFER | 
                    (BULK_TRANSFER_TYPE << EP_CTRL_BUFFER_TYPE_LSB) |
                    ((uint32_t)ep->buffer ^ (uint32_t)usb_dpram);
}

void usb_set_ep_available(end_point *ep) {
    ep->buf_ctrl->first |= 64 | USB_BUF_CTRL_AVAIL;
}

void usb_set_ep_double_buffered(end_point *ep) {
    *ep->ep_ctrl |= EP_CTRL_DOUBLE_BUFFERED_BITS;
}

// endpoint functions
void ep0_in_func(void) {
    if (change_address) {
        // if address change was requested we change it here
        usb_hw->dev_addr_ctrl = device_address;
        change_address = false;
    } else {
        usb_get(&ep0_out, NULL, 0);
    }
}

void ep0_out_func(void) {}

void usb_register_ep1_out_func(ep_func_ptr function) {
    user_ep1_func = function;
}

void usb_register_ep2_in_func(ep2_func_ptr function) {
    user_ep2_func = function;
}

void ep1_out_func(void) {
    uint8_t buf[64];
    uint8_t len = usb_get(&ep1_out, buf, 64);
    if (user_ep1_func == NULL) return;
    user_ep1_func(buf, &len);
}

void ep2_in_func(void) {
}

