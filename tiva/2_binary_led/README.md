# Four-bit binary LED counter

Small bare-metal program for the Texas Instruments EK-TM4C123GXL LaunchPad.
Each debounced press of the onboard SW1 button increments a four-bit counter
shown on LEDs connected to PB0-PB3.

## Connections

The TM4C123 uses 3.3 V logic. Do not apply 5 V to its GPIO pins.

| Binary weight | LaunchPad pin | External circuit |
| --- | --- | --- |
| 1 | PB0 | 220 ohm resistor, LED anode |
| 2 | PB1 | 220 ohm resistor, LED anode |
| 4 | PB2 | 220 ohm resistor, LED anode |
| 8 | PB3 | 220 ohm resistor, LED anode |

Connect each LED cathode to ground. SW1 is already connected to PF4 on the
LaunchPad and uses the microcontroller's internal pull-up resistor, so no
external button wiring is required.

## Toolchain

The project has no vendor SDK dependency. It requires:

- GNU Arm Embedded GCC
- GNU Make
- OpenOCD
- `gdb-multiarch` for command-line debugging

The linker script, startup code, interrupt vector table, and required
peripheral register definitions are included in the project.

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
programs `build/binary-led.elf` through the onboard ICDI debugger.

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

At reset, the firmware configures PB0-PB3 as digital outputs and PF4/SW1 as
an active-low digital input with an internal pull-up. All four LEDs illuminate
for one second as a startup test and then turn off. Each debounced SW1 press
increments the displayed value from 0 through 15 before wrapping to 0.

SysTick delays use the TM4C123's default 16 MHz system clock.
