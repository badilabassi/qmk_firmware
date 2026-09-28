# rd75

A customizable 84key keyboard.

![rd75](https://raw.githubusercontent.com/Linyer-qwq/image/main/rd75.jpg)

* Keyboard Maintainer: [Badi Labassi](https://github.com/badilabassi)
* Hardware Supported: Womier RD75 PCB with an ES32 FS026 microcontroller

Make example for this keyboard (after setting up your build environment):

    make wob/rd75:default

Flashing example for this keyboard:

    make wob/rd75:default:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Keyboard functions

As in Womier's firmware 0.1.5:

* **Fn + Del**: all lighting (keys and logo) on/off
* **Fn + H**, held 3 s: debounce 5 ms ↔ 2 ms; the logo flashes green (5 ms) or red (2 ms)
* **Fn + End** (`EE_CLR`), held 3 s: factory reset
* The logo lights white while Caps Lock is on

The firmware 0.1.5 radio-timer commands (`0x15` and `0x16`) are not sent. The
production RD75 radio firmware does not support either command; automatically
synchronizing them prevents both Bluetooth and 2.4 GHz HID reports. Fn+T
therefore retains its normal `T` behavior on the function layers.

## Bootloader

Enter the bootloader in 2 ways:

* **Bootmagic reset**: Hold down the key at (0,0) in the matrix (Esc key) and plug in the keyboard
* **Physical reset button**: Briefly press the button on the back of the PCB
