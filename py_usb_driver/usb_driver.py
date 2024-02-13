
import usb.core
import usb.util
import usb.backend.libusb1
import libusb_package
import os
os.environ['PYUSB_DEBUG'] = 'debug'

str = libusb_package.find_library("lib")
be = usb.backend.libusb1.get_backend(find_library=lambda x: str)
# find our device
dev = usb.core.find(idVendor=0x69, idProduct=0x42, backend=be)
# was it found?
if dev is None:
    raise ValueError('Device not found')


dev.set_configuration()
# get an endpoint instance
cfg = dev.get_active_configuration()
print(cfg)
intf = cfg[(0, 0)]

outep = usb.util.find_descriptor(
    intf,
    # match the first OUT endpoint
    custom_match= \
        lambda e: \
            usb.util.endpoint_direction(e.bEndpointAddress) == \
            usb.util.ENDPOINT_OUT)

inep = usb.util.find_descriptor(
    intf,
    # match the first IN endpoint
    custom_match= \
        lambda e: \
            usb.util.endpoint_direction(e.bEndpointAddress) == \
            usb.util.ENDPOINT_IN)

assert inep is not None
assert outep is not None

test_string = "Hello World!"
outep.write(test_string)
from_device = inep.read(len(test_string))

print("Device Says: {}".format(''.join([chr(x) for x in from_device])))