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

## Bootloader

Enter the bootloader in 2 ways:

* **Bootmagic reset**: Hold down the key at (0,0) in the matrix (Esc key) and plug in the keyboard
* **Physical reset button**: Briefly press the button on the back of the PCB
