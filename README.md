# rtos
A microkernel built for the Raspberry Pi 4 with an included train controller to control a Marklin trainset.

## Features
- Timer Interrupts
- UART Interrupts for Marklin controller and serial console
- IPC via message passing
- Console to set train speed, turnouts, with a live view of train statuses

## Building
1. Install the [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
2. Run `make` which will create an image, `kernel.img`
