# RD75 USB report diagnostics

These firmware images were built from commit `2fa2c2c716` on branch
`feat/rd75-qmk`, with the local `ES32_USB_USE_USB0 TRUE` correction present.
They use GNU Arm Embedded Toolchain 10.3-2021.10.

Flash and test them in numeric order. After each flash, select wired USB mode
with Fn+5, then test a normal letter key.

1. `01-baseline-qmk0345.bin`
   - Control build with the normal QMK 0.34.5 configuration.
   - Expected to reproduce the current failure.
2. `02-console-off.bin`
   - Only change from the control: `CONSOLE_ENABLE=no`.
   - If typing works, the extra console interface/endpoint is the trigger.
3. `03-nkro-off.bin`
   - Only change from the control: `NKRO_ENABLE=no`.
   - If typing works, the NKRO/shared HID endpoint path is the trigger.
4. `04-direct-usb-no-host-wrapper.bin`
   - Only change from the control: the RD75 timer does not replace QMK's
     ChibiOS host driver with `es_user_driver`.
   - This image is for wired diagnosis only. If typing works, the custom
     host-driver forwarding shim is the trigger.

If all four images enumerate but none send keys, the next isolation target is
the QMK/ChibiOS FS026 low-level USB and HAL implementation.

Results reported for images 1-4: all enumerate, none send normal keys.

5. `05-vendor-usb-lld.bin`
   - QMK 0.34.5 control with only the working RD75 project's FS026 USB
     low-level driver substituted.
   - This restores its endpoint-busy check and deferred USB-address sequence.
6. `06-vendor-fs026-hal-init.bin`
   - QMK 0.34.5 control with only the working RD75 project's FS026 clock/HAL
     initialization substituted.
7. `07-vendor-usb-and-hal.bin`
   - QMK 0.34.5 with both vendor FS026 substitutions from images 5 and 6.

For images 5-7, again select wired mode with Fn+5 and test a normal letter.
If only 5 works, the FS026 USB low-level changes are responsible. If only 6
works, clock/HAL initialization is responsible. If only 7 works, both changes
are required together. If none work, the remaining incompatibility is above
the FS026 low-level driver in QMK's ChibiOS USB protocol/request layer.

Results reported for images 5-7: image 5 works, image 6 does not, and image 7
works. This isolates the regression to the FS026 USB low-level driver.

8. `08-deferred-usb-address-only.bin`
   - Only restores the vendor driver's deferred handling of the USB
     `SET_ADDRESS` request. The new transmit-busy check remains unchanged.
9. `09-vendor-tx-busy-check-only.bin`
   - Only restores the vendor driver's endpoint transmit-busy helper. The new
     immediate USB-address handling remains unchanged.

If image 8 works and image 9 does not, the exact regression is committing the
new USB device address too early, before EP0 finishes the status stage. The
permanent fix should then be limited to the two address-handling hunks.

Results reported for images 8-9: image 8 works and image 9 does not. The
regression is therefore confirmed to be the premature USB device-address
write. The two address-handling hunks from image 8 are the permanent fix.

## Permanent implementation

The fix is scoped to the RD75 instead of modifying the `lib/chibios-contrib`
submodule. `keyboards/wob/rd75/platform.mk` includes the standard FS026
platform and replaces only its USB low-level-driver source with
`keyboards/wob/rd75/hal_usb_lld.c`. That local driver differs from the standard
driver only in the confirmed deferred-address handling (plus normalization of
one malformed source comment).

The resulting default firmware is byte-for-byte identical to image 8.

## Wireless regression diagnostics

Image 8 was also confirmed to work over both 2.4 GHz and Bluetooth, while the
firmware containing the imported 0.1.5 changes does not. This isolates the
wireless regression to commit `18b4a32407`.

10. `10-no-radio-timer-sync.bin`
    - Keeps the 0.1.5 firmware behavior and RD75-scoped USB fix.
    - Disables only automatic synchronization of the new radio timer fields
      through commands `0x15` and `0x16`.
    - Test both Fn+4 (2.4 GHz) and Fn+1 (Bluetooth).

Results reported for image 10: both 2.4 GHz and Bluetooth work. The regression
is therefore caused by the automatic synchronization of at least one new 0.1.5
radio timer field.

11. `11-no-sleep-timer-sync.bin`
    - Disables only automatic sleep-time synchronization through command
      `0x15`; secondary RF-timer synchronization remains enabled.
12. `12-no-secondary-rf-timer-sync.bin`
    - Disables only automatic secondary RF-timer synchronization through
      command `0x16`; sleep-time synchronization remains enabled.

Test both wireless modes in images 11 and 12. If only one works, the field
disabled in that image is the incompatible one. If neither works, both fields
must be disabled for this radio firmware.

Results reported for images 11-12: neither works. Commands `0x15` and `0x16`
are each independently incompatible with the production RD75 radio firmware.

The permanent implementation removes every path that can emit either command,
including automatic status synchronization and the Fn+T sleep-time command.
The remaining firmware 0.1.5 changes are retained.
