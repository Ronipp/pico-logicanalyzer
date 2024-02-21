#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include "pico/stdlib.h"

#define DEVICE_DESCRIPTOR_TYPE 0x01
#define CONFIGURATION_DESCRIPTOR_TYPE 0x02
#define STRING_DESCRIPTOR_TYPE 0x3
#define INTERFACE_DESCRIPTOR_TYPE 0x04
#define ENDPOINT_DESCRIPTOR_TYPE 0x05
#define QUALIFIER_DESCRIPTOR_TYPE 0x06

#define BULK_TRANSFER_TYPE 0x2
#define CONTROL_TRANSFER_TYPE 0x0
#define DEVICE_STATUS 0x1

#define USB_DIR_IN 0x80
#define USB_DIR_OUT 0x0
#define MS_REQUEST_TYPE 0xc0
#define MS_EXT_PROP_REQUEST 0xc1

#define MAX_PACKET_SIZE 64

#define USB_SPECIFICATION_NUMBER 0x0200
#define VENDOR_SPECIFIC 0xff
#define CONFIG_ATTRIBUTES 0xc0
#define MAXPOWER_100MA 0x32

#define REQUEST_GET_STATUS 0x0
#define REQUEST_CLEAR_FEATURE 0x01
#define REQUEST_SET_FEATURE 0x03
#define REQUEST_SET_ADDRESS 0x05
#define REQUEST_GET_DESCRIPTOR 0x06
#define REQUEST_SET_DESCRIPTOR 0x07
#define REQUEST_GET_CONFIGURATION 0x08
#define REQUEST_SET_CONFIGURATION 0x09

#define RONALDS_VENDOR_ID 0x69
#define RONALDS_PRODUCT_ID 0x42

#define LANG_US 0x0409

#define MS_OS_INDEX 0x0004 // from documentation

// https://www.beyondlogic.org/usbnutshell/usb5.shtml

// note: structs are packed since we need to send the stuff in specific way.
// packed zeroes would mess up the conversation between host and device.

/*
The device descriptor of a USB device represents the entire device.
As a result a USB device can only have one device descriptor.
It specifies some basic, yet important information about the device
such as the supported USB version, maximum packet size, vendor and 
product IDs and the number of possible configurations the device can have.
*/
typedef struct {
    uint8_t bLength; // Size of the Descriptor in Bytes
    uint8_t bDescriptorType; // Device Descriptor (0x01)
    uint16_t bcdUSB; // USB Specification Number which device complies to. 0x0110 for pico
    uint8_t bDeviceClass; // class code 0xFF for vendor specified
    uint8_t bDeviceSubClass; // Subclass Code
    uint8_t bDeviceProtocol; // protocol code
    uint8_t bMaxPacketSize; // Maximum Packet Size for Zero Endpoint this should be 64
    uint16_t idVendor; // vendor ID this can be whatever
    uint16_t idProduct; // product ID this can be whatever
    uint16_t bcdDevice; // device release number
    uint8_t iManufacturer; // index of manufacturer string Descriptor can be 0
    uint8_t iProduct; // index of product string Descriptor can be 0
    uint8_t iSerialNumber; // index of serial number string Descriptor can be 0
    uint8_t bNumConfigurations; // number of possible configurations
} __packed device_descriptor;


/*
A USB device can have several different configurations although the
majority of devices are simple and only have one. The configuration
descriptor specifies how the device is powered, what the maximum
power consumption is, the number of interfaces it has. Therefore it
is possible to have two configurations, one for when the device
is bus powered and another when it is mains powered. As this is a 
"header" to the Interface descriptors, its also feasible to have one
configuration using a different transfer mode to that of another configuration. 
*/
typedef struct {
    uint8_t bLength; // Size of the Descriptor in Bytes
    uint8_t bDescriptorType; // configuration descriptor 0x02
    uint16_t wTotalLength; // total length in bytes of data returned, reflects the number of bytes in the hierarchy.
    uint8_t bNumInterfaces; // number of interfaces
    uint8_t bConfigurationValue; // value to use as an argument to select this config
    uint8_t iConfiguration; // Index of String Descriptor describing this configuration should be 0
    uint8_t bmAttributes; // bitmap d0-4 reserved set 0, d5 remote wakeup, d6 self powered, d7 reserved set to 1
    uint8_t bMaxPower; // value in mA, maximum power consumption
} __packed configuration_descriptor;


/*
The interface descriptor could be seen as a header or grouping of
the endpoints into a functional group performing a single feature of the device.
*/
typedef struct {
    uint8_t bLength; // Size of the Descriptor in Bytes
    uint8_t bDescriptorType; // interface descriptor 0x04
    uint8_t bInterfaceNumber; // number of interface
    uint8_t bAlternateSetting; // value used to select alternative setting
    uint8_t bNumEndpoints; // number of endpoints used for this interface
    uint8_t bInterfaceClass; // class code
    uint8_t bInterfaceSubClass; // subclass code
    uint8_t bInterfaceProtocol; // protocol code
    uint8_t iInterface; // index of string descriptor
} __packed interface_descriptor;

/*
Endpoint descriptors are used to describe endpoints other than endpoint zero.
Endpoint zero is always assumed to be a control endpoint and is configured
before any descriptors are even requested. The host will use the information
returned from these descriptors to determine the bandwidth requirements of the bus.
*/
typedef struct {
    uint8_t bLength; // size of descriptor in bytes
    uint8_t bDescriptorType; // Endpoint descriptor 0x05
    uint8_t bEndpointAddress; // endpoint address bits 0-3 endpoint number, bits 4-6 reserved set to 0, bit 7 direction 0=out 1=in
    uint8_t bmAttributes; // bitmap see below
    uint16_t wMaxPacketSize; // maximum packet size for the endpoint should be 64
    uint8_t bInterval; // Interval for polling endpoint data transfers. Value in frame counts. Ignored for Bulk & Control Endpoints
} __packed endpoint_descriptor;
/*
BMATTRIBUTES
Bits 0..1 Transfer Type
    00 = Control
    01 = Isochronous
    10 = Bulk
    11 = Interrupt
Bits 2..7 are reserved. If Isochronous endpoint,
Bits 3..2 = Synchronisation Type (Iso Mode)
    00 = No Synchonisation
    01 = Asynchronous
    10 = Adaptive
    11 = Synchronous
Bits 5..4 = Usage Type (Iso Mode)
    00 = Data Endpoint
    01 = Feedback Endpoint
    10 = Explicit Feedback Data Endpoint
    11 = Reserved
*/

typedef struct {
   uint8_t bmRequestType; // bitmap of request type see below 
   uint8_t bRequest; // request being made
   uint16_t wValue; // something to do with parameters
   uint16_t wIndex; // something to do with parameters
   uint16_t wLength; // number of bytes of parameter data
} __packed usb_setup_packet;
/*
BMREQUESTTYPE
D7 Data Phase Transfer Direction
0 = Host to Device
1 = Device to Host
D6..5 Type
0 = Standard
1 = Class
2 = Vendor
3 = Reserved
D4..0 Recipient
0 = Device
1 = Interface
2 = Endpoint
3 = Other
4..31 = Reserved
*/

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType; // 0x3
    uint16_t wLANGID0;
} __packed language_descriptor;

typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
} __packed string_descriptor_head;


/*
MS DESCRIPTORS
*/
typedef struct {
    uint8_t bLength; // should be 0x12
    uint8_t bDescriptorType; // string 0x3
    uint8_t qwSignature[14]; // buffer for the magic string MSFT100
    uint8_t bMS_VendorCode; // vendor code used to get next descriptors
    uint8_t bPad; // padding should be 0
} __packed ms_os_string_descriptor;

typedef struct {
    uint32_t dwLength; // size
    uint16_t bcdVersion; // descriptors version
    uint16_t wIndex; // set to 0x04
    uint8_t bCount; // number of properties should set to 1
    uint8_t reserved[7]; // 7 bytes of reserved should set to 0
    uint8_t bFirstInterfaceNumber; // first interface should be 0
    uint8_t reserve; // one byte of reserved
    uint8_t compatibleID[8]; // compatible id string should be "WINUSB00"
    uint8_t subCompatibleID[8]; // shoud be set to all zeros
    uint8_t reserv[6]; // a lot of reserved
} __packed winsub_descriptor;

typedef struct {
    uint32_t dwLength;
    uint16_t bcdVersion; // descriptor version
    uint16_t wIndex; // should be 0x5?
    uint16_t wCount; // should be 0?
} __packed ms_extended_properties_descriptor;


typedef struct {
    uint16_t first;
    uint16_t second;
} __packed buf_ctrl_struct;
/*
endpoint struct
*/
typedef struct {
    uint8_t number; // number of this endpoint
    uint8_t pid; // next pid to use
    volatile uint8_t *buffer; // the dpram buffer location
    volatile uint8_t *buffer_second; // second buffer location
    volatile uint32_t *ep_ctrl; // the ep control register location
    volatile buf_ctrl_struct *buf_ctrl; // the buffer control registers
} end_point;

#endif