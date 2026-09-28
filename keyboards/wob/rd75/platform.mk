# Start with the standard FS026 platform, then replace only its USB low-level
# driver with the RD75-scoped copy containing the confirmed SET_ADDRESS fix.
include $(CHIBIOS_CONTRIB)/os/hal/ports/ES32/FS026/platform.mk

FS026_USB_LLD := $(CHIBIOS_CONTRIB)/os/hal/ports/ES32/LLD/USBv1/hal_usb_lld.c
PLATFORMSRC := $(patsubst $(FS026_USB_LLD),$(KEYBOARD_PATH_1)/hal_usb_lld.c,$(PLATFORMSRC))
