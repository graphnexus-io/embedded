# Arduino Sokoban 100

Sokoban 100 is a standalone, fixed-memory Sokoban firmware for the Arduino
Mega 2560 and a 480x320 ST7796S SPI TFT. It provides 100 deterministic,
solvable levels, Free Select and Campaign modes, complete joystick navigation,
serial controls, undo, scoring, EEPROM resume/progress, and a steady
full-backlight configuration.

This is a separate project from `12_arduino_retro_games`; it contains only the
enhanced Sokoban game.

## Hardware and wiring

- Arduino Mega 2560 / ATmega2560 at 16 MHz
- USB serial at 115200 baud
- 4-inch ST7796S TFT, 480x320 landscape
- LCDWIKI_GUI and LCDWIKI_SPI

```text
LCD_CS   -> D10
LCD_RST  -> D8
LCD_RS   -> D9
LCD_LED  -> D5 (backlight enable)
SD_CS    -> D4
MOSI     -> D51
MISO     -> D50
SCK      -> D52
```

The TFT uses D50-D52 and D10. The onboard SD reader is not initialized or
touched by this firmware. D53 is held high as an output so the Mega remains SPI
master. Touch is not initialized.

Standard two-axis joystick module:

```text
SW       -> D2
URY/VRY  -> A0
URX/VRX  -> A1
VCC/+5V  -> 5V
GND      -> GND
```

The switch uses the Mega's internal pull-up and is active-low; no external
switch resistor is required.

## Build and upload

The tested environment is Arduino CLI 1.5.1 with Arduino AVR Boards 1.8.8.
LCDWIKI GUI 1.0 and LCDWIKI SPI are vendored in `../libraries/`.

Install the board core once:

```sh
arduino-cli core install arduino:avr@1.8.8
```

From this project directory:

```sh
arduino-cli compile \
  --fqbn arduino:avr:mega:cpu=atmega2560 \
  --libraries ../libraries \
  ./sketch
```

Upload, changing the serial port if necessary:

```sh
arduino-cli upload \
  --fqbn arduino:avr:mega:cpu=atmega2560 \
  --port /dev/ttyACM0 \
  --verify \
  ./sketch
```

## Main menu and modes

- **Continue:** restores the active board, player/crate positions, mode, level,
  moves, pushes, and score saved during the previous session.
- **Free Select:** allows any level from 001 through 100. Left/Right changes
  one level and Up/Down changes ten.
- **Campaign:** starts at level 001 and unlocks the next level only after the
  current level is solved.

Starting another level replaces the one active-board save. Campaign progress
and its cumulative score remain available while playing Free Select.

## Controls

The joystick controls the entire interface:

- Move the stick to move the player or navigate menus.
- Hold the stick to repeat after 330 ms and then every 150 ms.
- Short-press the switch to select; during play it pauses and while paused it
  resumes.
- Hold the switch for 900 ms to go back.

Keep the stick centered during power-up/reset. Startup averages 16 samples from
each axis. If an axis is reversed, change `JOYSTICK_REVERSE_X` or
`JOYSTICK_REVERSE_Y` in `sketch/config.h`.

USB serial works in parallel and accepts lowercase/uppercase letters and ANSI
arrow-key sequences:

- W/A/S/D or arrow keys: move/navigate
- U: undo, up to 64 moves in the current run
- P: pause/resume
- R: restart
- Q or Esc: back
- Enter: select/continue

The undo history is SRAM-only and is not restored after reset.

## Backlight

LCDWIKI leaves D5 steadily HIGH (255). The firmware never calls
`analogWrite()`, so no backlight PWM runs while TFT bytes are transferred. No
display settings are loaded or saved. Gameplay progress continues to use the
Mega's EEPROM journal.

## Levels and difficulty

All levels use a 16x11 grid:

- 001-005: **Beginner**, two crates, open layouts, 2-6 certified pulls.
- 006-010: **Medium**, three crates, 6-14 internal wall cells, 15-35 pulls.
- 011-100: **Super Expert**, 4-6 crates, 20-34 internal wall cells, 70-140
  pulls, and increasingly strict crate displacement, turn, and distance floors.

The Super Expert levels average about 70 certified direction turns and a
crate-to-target assignment lower bound above 30 pushes. These metrics are much
higher than the former 500-level set, although exact human solving difficulty
can still vary between generated boards.

The firmware does not store large text maps. It deterministically builds a
connected board, starts from a solved crate arrangement, and performs legal
reverse pulls. Reversing those pulls proves that each accepted starting board
has a solution. A 100-byte recipe table in program flash selects the validated
candidate immediately, avoiding long generation delays on the 16 MHz MCU.

Run the complete host validation with:

```sh
g++ -std=c++11 -Wall -Wextra -Werror \
  -Itests -Isketch \
  tests/level_generator_test.cpp sketch/level_generator.cpp \
  -o /tmp/sokoban_100_level_test
/tmp/sokoban_100_level_test
```

It checks all 100 layouts for valid coordinates, crate/target counts, unsolved
starts, uniqueness, one-attempt recipes, and each difficulty-band contract.

## Persistence

The rotating CRC-16 EEPROM journal stores the current board, selected mode and
level, Free Select choice, Campaign unlock/score, player and crate positions,
moves, pushes, and level score. Movement is saved after 900 ms of inactivity;
important transitions save immediately. This avoids blocking each move and
spreads writes over the Mega's 4 KiB EEPROM.

Firmware 2.0 uses a new six-crate record. It recognizes the old 40-byte v1/v2
500-level records, preserves their mode, clamped level choice, campaign unlock,
and campaign score, and discards the incompatible active board. The next save
uses the v3 layout.

## Architecture

```text
sketch/sketch.ino          application state flow and save scheduling
sketch/app_ui.*            main menu, level browser, completion screens
sketch/sokoban_game.*      rules, undo, score, incremental board rendering
sketch/level_generator.*  deterministic solvable level generation
sketch/persistence.*       rotating CRC-checked EEPROM journal and migration
sketch/session.h           portable saved-session data model
sketch/serial_input.*      nonblocking serial and ANSI-key parser
sketch/joystick_input.*    calibrated axes, hysteresis, repeat, button gestures
sketch/display_hal.*       LCDWIKI/ST7796S boundary and shared-SPI restoration
sketch/config.h            pins, geometry, limits, palette, and version
tests/*                    host validation for all 100 level seeds
```

There is no Arduino `String`, dynamic allocation, recursion, framebuffer,
blocking input wait, or touch support. All game and parser buffers have fixed
compile-time bounds. Normal movement redraws only affected
cells and the statistics line.

## Known limitations

- Levels are deterministic generated puzzles, not hand-authored maps.
- Difficulty is enforced through construction metrics; no full optimal solver
  runs on the Arduino.
- There is no deadlock detector, hint solver, or persisted undo history.
- Only one active board can be continued.
- Hardware testing is still required for joystick thresholds, visual tuning,
  and real power-interruption behavior.
