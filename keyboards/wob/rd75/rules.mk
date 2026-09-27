# QMK does not yet register FS026 as a processor token, so select the
# architecture and ChibiOS-Contrib platform locally for this keyboard.
MCU = cortex-m0
ARMV = 6
MCU_FAMILY = ES32
MCU_SERIES = FS026
MCU_LDSCRIPT = FS026
MCU_STARTUP = FS026

SRC += rdr_common.c

# Debounce time switchable at run time (Fn+H), see rd75_debounce.c.
SRC += rd75_debounce.c
