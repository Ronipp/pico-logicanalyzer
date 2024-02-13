
import usb.core
import usb.util
import libusb

# find our device
dev = usb.core.find(idVendor=0x0069, idProduct=0x0042)
print(dev)
# was it found?
if dev is None:
    raise ValueError('Device not found')

# get an endpoint instance
cfg = dev.get_active_configuration()
intf = cfg[(0, 0)]
print(cfg)

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