# SN74HC74 flip-flop exploration

Small bare-metal program for the Texas Instruments EK-TM4C123GXL LaunchPad.
It drives the data and clock inputs of one SN74HC74 D-type flip-flop, reports
the two test transitions over UART0, and then blinks the board's red LED.

## Connections

The TM4C123 uses 3.3 V logic. Do not apply 5 V to its GPIO pins.

| Signal | LaunchPad pin | SN74HC74 pin |
| --- | --- | --- |
| Data | PB0 | 1D, pin 2 |
| Clock | PB1 | 1CLK, pin 3 |
| 3.3 V | 3V3 | 1PRE and 1CLR, pins 4 and 1 |
| Ground | GND | GND, pin 7 |

Connect the SN74HC74 supply pin 14 to 3.3 V. The first flip-flop outputs are 1Q
on pin 5 and 1/Q on pin 6. Place a 100 nF decoupling capacitor across the
chip's supply pins. Tie every unused input on the second flip-flop to a defined
logic level; never leave CMOS inputs floating.

UART0 TX is available on PA1 at 115200 baud. The LaunchPad debug USB connection
normally exposes this UART as a serial port. PF1 is the onboard red LED.

## Toolchain

The project has no vendor SDK dependency. It was tested with:

- GNU Arm Embedded GCC 14.2.1
- GNU Make
- OpenOCD 0.12.0
- `gdb-multiarch` for command-line debugging

The linker script, startup code, interrupt vector table, and peripheral register
definitions are included in the project.

## Build

```sh
make
```

The ELF, BIN, HEX, disassembly, and linker map are written to `build/`.

## Flash

Connect the LaunchPad's debug USB port and run:

```sh
make flash
```

The Makefile uses OpenOCD's `board/ti_ek-tm4c123gxl.cfg` configuration and
programs `build/sn74hc74-test.elf` through the onboard ICDI debugger.

## Debug

Start OpenOCD in one terminal:

```sh
make openocd
```

Start GDB in a second terminal:

```sh
make debug
```

Then connect from GDB:

```text
target extended-remote :3333
monitor reset halt
load
break main
continue
```

## Program sequence

At reset, the firmware configures PB0 as data, PB1 as clock, PF1 as the status
LED, and PA1 as UART0 TX. It clocks in a zero, waits, clocks in a one, and then
blinks PF1 forever. The delay loops are approximate and depend on the default
16 MHz system clock.
