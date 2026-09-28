# QMK does not yet register FS026 as a processor token, so select the
# architecture and ChibiOS-Contrib platform locally for this keyboard.
MCU = cortex-m0
ARMV = 6
MCU_FAMILY = ES32
MCU_SERIES = FS026
MCU_LDSCRIPT = FS026
MCU_STARTUP = FS026

# Use the RD75-scoped FS026 USB LLD. The ChibiOS-Contrib implementation
# commits SET_ADDRESS before EP0's status stage has completed on this MCU.
PLATFORM_MK = $(KEYBOARD_PATH_1)/platform.mk

VPATH += keyboards/wob/common
SRC += rdmctmzt_common.c
SRC += three_mode.c
SRC += user_battery.c
SRC += user_eeprom.c
SRC += user_emi.c
SRC += user_led_custom.c
SRC += user_spi.c
SRC += user_system.c

# Debounce time switchable at run time (Fn+H), see rd75_debounce.c.
SRC += rd75_debounce.c
