# MiniOS Embedded Monitor

This Arduino Mega 2560 firmware is a fixed-memory educational embedded-system monitor. USB serial supplies command input at 115200 baud, and all command output is mirrored to a 480x320 ST7796S SPI TFT. The TFT preserves a restrained early-UNIX glass-terminal appearance: black background, pale phosphor text, one thin frame, and no title, footer, widgets, touch UI, or framebuffer.

Firmware version: 0.6.0. The prompt on Serial and TFT is exactly `i: `.

## Hardware and wiring

- Arduino Mega 2560 / ATmega2560 at 16 MHz
- 4-inch ST7796S SPI TFT in 480x320 landscape orientation
- `LCD_CS` -> D10
- `LCD_RS` / DC -> D9
- `LCD_RST` -> D8
- `LCD_LED` -> D5
- MOSI -> D51
- MISO -> D50
- SCK -> D52
- TFT onboard MicroSD `SD_CS` -> D4

The LCD and MicroSD reader share MOSI D51, MISO D50, and SCK D52. Their
separate active-low chip selects must never be active together: LCD activity
keeps D4 high, while SD activity keeps D10 high. D53 is configured as an output
and held high so the ATmega2560 hardware SPI controller remains in master mode.
Touch is not initialized. MicroSD access is available through the bounded
filesystem commands and the two existing on-demand diagnostics described
below; no card operation runs at startup.

## Line editor

- Left/Right move the underscore cursor through the active command.
- Up/Down navigate the eight-entry command history.
- Down past the newest history entry restores the line that was being edited.
- Typing inserts at the cursor.
- Backspace removes the character before the cursor.
- Delete removes the character under the cursor.
- CR, LF, and CRLF submit commands; CRLF executes only once.
- Command input is always null-terminated and limited to 95 characters.

The editor recognizes normal ANSI cursor sequences (`ESC [ A/B/C/D`), the common SS3 variants, and `ESC [ 3 ~` for Delete. Recalled history lines remain editable, and replacement clears stale Serial and TFT characters.

## Parsing

Each command is tokenized in place into at most 12 `argc`/`argv` entries. Repeated whitespace is ignored, double quotes group arguments, outer quotes are removed, and `""` creates an empty argument. Escaping inside quotes is intentionally not implemented. An unmatched quote or an excessive argument count produces a specific error.

Up to six commands can be placed on one line with semicolons:

```text
i: echo one; echo two; uptime
i: echo "one;two"; echo three
```

Semicolons inside quotes remain part of the argument. Subcommands execute left to right, and the shell retains the final subcommand's `CommandStatus`.

## Commands

Use `help` to list commands and `help <command>` for command-specific usage and notes.

```text
help [command]
echo [text...] [> file | >> file]
pwd
cd [directory]
ls [directory]
cat <file>
mkdir <directory>
rmdir <directory>
rm <file>
mv <source> <destination>
nano <file>
clear
version
uptime
info
history
mem
backlight [0-255]
sdinfo
sdtest
reboot
gpio mode <pin> input|input_pullup|output
gpio read <pin>
gpio write <pin> 0|1|low|high
display clear
display fill <colour|rgb565>
display info
display benchmark
calc <value> <operator> <value>
```

### Examples

```text
i: echo "hello from unix"
hello from unix

i: mkdir DATA
Directory created: /DATA
i: echo "hello storage" > DATA/NOTE.TXT
Wrote 14 bytes to /DATA/NOTE.TXT
i: cat DATA/NOTE.TXT
hello storage
i: cd DATA
i: pwd
/DATA
i: mv NOTE.TXT RENAMED.TXT
Moved: /DATA/NOTE.TXT -> /DATA/RENAMED.TXT
i: nano CONFIG.JSN

i: calc 2147483647 + 1
Error: arithmetic overflow
Usage: calc <value> <operator> <value>

i: calc 0x20 | 0x04
36

i: gpio mode 22 output
OK
i: gpio write 22 high
OK
i: gpio read 22
22: HIGH
```

The calculator accepts signed decimal and `0x`/`0X` hexadecimal operands. Operators are `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, `<<`, and `>>`. Arithmetic overflow, division/modulo by zero, malformed integers, and shifts outside 0..31 are rejected. Bitwise operations and shifts use defined `uint32_t` behavior.

`mem` reports the 8192-byte SRAM total and an instantaneous estimate between the current heap end and stack. It performs no allocation while measuring.

`backlight` reports the current TFT backlight PWM level. `backlight <level>` sets D5 from fully off (`0`) through fully on (`255`). D5 remains protected from all generic `gpio` commands.

`reboot` flushes Serial and resets through the AVR watchdog. It does not jump directly to address zero.

## Minimal filesystem shell

The filesystem commands use a small shared layer that owns SD readiness, path
validation, chip-select coordination, and all card operations. The first
filesystem command initializes the card. Later commands retain that verified
mount and force a real root-directory read before each operation. If that
probe fails, the layer makes one controlled reinitialization attempt, including
the documented 2 MHz fallback. Reliable arbitrary hot swapping is not promised
by the standard SD library; remove or insert a card only while no operation is
running.

Supported syntax is deliberately small:

```text
ls
ls /
ls <directory>
pwd
cd [directory]
cat <file>
echo [text...]
echo [text...] > <file>
echo [text...] >> <file>
mkdir <directory>
rmdir <directory>
rm <file>
mv <source> <destination>
nano <file>
```

The working directory starts at `/` and is held in one fixed 65-byte buffer.
`pwd` prints it. `cd` with no argument returns to `/`; otherwise it verifies
that the requested directory exists before changing state. Absolute paths
begin with `/`, while relative paths start at the working directory. `.` and
`..` are resolved without allowing navigation above `/`. Repeated `/`
separators are collapsed and trailing separators are removed. Empty paths,
control characters, invalid FAT components, and normalized paths longer than
64 characters are rejected. With no argument, `ls` lists the working
directory.

The installed SD 1.3.0 implementation supports FAT 8.3 names only. Each path
component is therefore checked as a one-to-eight-character base plus an
optional one-to-three-character extension. Case is preserved in shell status
messages, but FAT lookup is case-insensitive and directory listings normally
show uppercase short names. Spaces, long filenames, and FAT-illegal characters
are rejected instead of being silently truncated.

`ls` streams immediate entries in card order, without recursion or sorting.
It never stores a whole directory in SRAM. `cat` streams through a 64-byte
buffer and reads at most 4096 raw file bytes. CRLF is rendered as one newline,
standalone CR becomes a newline, and bytes outside printable ASCII or LF become
`.`. Files beyond the limit end with `[output truncated]`.

Echo redirection is available only to `echo`, and `>` or `>>` must be separate
tokens. `>` removes an existing regular file and creates a replacement;
`>>` creates or appends. Both write exactly one LF after the reconstructed text
and report a byte count that includes that newline. Therefore both
`echo "" > EMPTY.TXT` and `echo > EMPTY.TXT` create a one-byte file. Overwrite
is not atomic: if creation or writing fails after removal, the previous content
has already been lost.

`mkdir` creates only the requested directory after verifying that its immediate
parent already exists. This check prevents the standard library's internal
`mkdir -p` behavior from creating missing parents. `rmdir` opens the target and
uses one `openNextFile()` call to prove it is empty before removal; it never
recurses and always refuses `/`. `rm` removes exactly one regular file and
rejects directories.

The installed Arduino SD API has no rename primitive, so `mv` implements a
conservative regular-file move as copy, byte-for-byte verification, then source
removal. It never overwrites an existing destination and does not move
directories or interpret a directory destination as an implicit filename. A
failed copy or verification removes the newly created partial destination when
possible. This operation is not atomic: power loss can leave a partial
destination, and a source-removal failure deliberately leaves both verified
copies. There are no wildcards, multiple-file operations, `touch`, `cp`,
directory moves, `find`, pipelines, or general shell redirection.

## Small text editor

`nano <file>` opens an existing text file or starts a new empty file at that
path. It is a deliberately small modal editor, inspired by nano rather than a
compatible reimplementation. The Serial terminal and TFT show the same
30-row view. The TFT renderer compares rows and redraws only changed text, so
normal typing does not clear or flash the entire display.

Editor controls:

- printable ASCII inserts at the cursor;
- Enter inserts an LF newline;
- Left/Right and Up/Down move through the text;
- Backspace removes the character before the cursor;
- Delete removes the character under the cursor;
- Ctrl+O saves; Ctrl+S is accepted as an alternate save key when the terminal
  does not reserve it for software flow control;
- Esc exits immediately when unchanged;
- with unsaved changes, the first Esc warns and a second Esc discards; any
  editing or navigation key cancels that warning.

The editor holds at most 1024 text bytes plus a terminator in a fixed SRAM
buffer. Existing files larger than that are refused rather than truncated.
Input and loaded files support printable ASCII and newlines; tabs, binary data,
Unicode, search, selection, copy/paste commands, and syntax highlighting are
not implemented. CRLF and standalone CR are normalized to LF when loaded, and
saving writes the normalized buffer exactly. A new empty file is not created
until Ctrl+S or Ctrl+O is pressed.

Saving removes an existing file, writes the current buffer, then reopens it and
compares every byte. This detects incomplete writes, but it is not atomic:
power loss or card failure after removal can lose the previous file. File paths
still use the documented FAT 8.3 restrictions, for example `CONFIG.JSN` rather
than `config.json`.

## SD-card diagnostics

The diagnostics use the same readiness and bus-coordination layer as the
filesystem commands. They deliberately start and end a fresh SD session so
their initialization/fallback reports and TFT restoration behavior remain
deterministic. Use a FAT16 or FAT32 volume; exFAT is not supported by the
standard library.

`sdinfo` is non-destructive. It initializes the card, opens `/`, lists each
root entry as a file or directory, includes file sizes, closes every handle,
and restores the TFT terminal. Listing is deliberately limited to the root.

`sdtest` is a controlled destructive test of one reserved path only:
`/SDTEST.TXT`. It removes a stale test file if present, writes and verifies the
first LF-terminated line, appends and verifies the second line, reports the
exact resulting size, then deletes the test file and verifies deletion. It
stops at the first failed prerequisite stage. No other file or directory is
created, changed, or removed.

```text
i: sdinfo
i: sdtest
```

The library negotiates card startup at low speed and normally uses a 4 MHz data
clock. If its default initialization fails, the firmware reports the failure
and explicitly retries volume/root access at 2 MHz before giving up. Both paths
leave the SD chip select high and redraw the TFT before returning to `i: `.

## GPIO safety policy

The GPIO monitor validates Mega digital pin numbers and tracks which pins it configured as outputs during the current session. A write is denied until `gpio mode <pin> output` succeeds. Every GPIO operation rejects these reserved pins:

- D0, D1: UART RX/TX
- D4: SD card chip select
- D5: TFT backlight
- D8: TFT reset
- D9: TFT RS/DC
- D10: TFT chip select
- D50: SPI MISO
- D51: SPI MOSI
- D52: SPI SCK
- D53: AVR hardware SS / SPI master safety

This policy prevents monitor commands from breaking USB communication or the active display bus.

## Display diagnostics

Named fill colours are `black`, `white`, `red`, `green`, `blue`, and `yellow`; 16-bit RGB565 values such as `0x001F` and `0xF800` are also accepted. A fill is displayed for approximately 350 ms, after which the complete terminal state is restored.

`display benchmark` measures a full-screen fill, complete terminal redraw, and representative text-line render with `micros()`. It restores the terminal before printing results. The diagnostic operations are intentionally blocking while the measurement or short fill demonstration runs; normal line editing remains non-blocking.

## Fixed-memory architecture

- `sketch.ino`: startup and continuous shell polling
- `command_shell`: serial editor, ANSI state machine, history, semicolon splitting, in-place tokenization, submission, and last status
- `commands`: flash-backed command table, core command handlers, validation, and status returns
- `commands_filesystem`: filesystem argument checking, echo-redirection parsing, precise errors, and shell-facing output
- `text_editor`: modal input state, 1 KiB text buffer, viewport, cursor movement, incremental Serial/TFT rendering, and save status
- `console`: output mirrored to Serial and TFT, including bounded integer conversion
- `tft_terminal`: 30x76 text ring, dirty-line rendering, cursor, scrolling, and diagnostics
- `hardware_services`: SRAM estimate, watchdog reboot, reserved-pin policy, and session GPIO state
- `filesystem`: readiness/retry state, fixed working directory, 8.3 path normalization, streaming I/O, verified mutations, and shared-SPI coordination
- `sd_diagnostics`: root listing, staged file verification, cleanup, and TFT restoration using shared SD readiness
- `config.h`: central firmware identity and all fixed limits

Important fixed storage includes a 96-byte command line, 12 argument pointers on the temporary command stack, eight 64-byte history slots, a 96-byte saved edit line, a 30x77-byte TFT line ring, a 65-byte working directory, and the editor's 1025-byte text buffer plus 65-byte path. Filesystem call stacks use bounded 65-byte paths and 64-byte I/O blocks; `mv` keeps one copy block and two comparison blocks while it runs, but never buffers a complete file. Directory listings are also streamed. Project code uses no `String`, explicit dynamic allocation, recursion, RTOS, or full-screen framebuffer. The standard Arduino SD 1.3.0 `File` implementation itself obtains a small `SdFile` handle with `malloc()` and releases it with `free()` on `File::close()`; every filesystem and diagnostic handle is closed promptly. This is a limitation of the required standard library, not an allocation made by the MiniOS source.

## Build and upload

The tested command-line environment is Arduino CLI 1.5.1 with Arduino AVR
Boards 1.8.8 and SD 1.3.0. The required LCDWIKI GUI 1.0 and LCDWIKI SPI
sources are included in `../libraries/` together with their upstream licenses.

Install the board core and SD library once:

```sh
arduino-cli core install arduino:avr@1.8.8
arduino-cli lib install SD@1.3.0
```

From this project directory, compile with the bundled display libraries:

```sh
arduino-cli compile \
  --fqbn arduino:avr:mega:cpu=atmega2560 \
  --libraries ../libraries \
  ./sketch
```

Replace `/dev/ttyACM0` if the Mega uses another port:

```sh
arduino-cli upload \
  --fqbn arduino:avr:mega:cpu=atmega2560 \
  --port /dev/ttyACM0 \
  ./sketch
```

## Known limitations

- History is volatile, stores eight entries, and truncates stored entries to 63 characters.
- The parser has no escape sequences, precedence, parentheses, pipes, variables, wildcard expansion, or scripting. Redirection is recognized only by `echo` when `>` or `>>` is a standalone token.
- Only the active command supports cursor editing; there is no multi-line screen navigation.
- TFT scrolling redraws the fixed text grid because there is no framebuffer.
- Diagnostic fill and benchmark operations briefly block serial polling.
- GPIO mode tracking represents commands issued in the current session, not external register changes.
- The standard SD library supports FAT16/FAT32 and 8.3 short names, not exFAT or long filenames. A roughly 58 GiB SDXC card must use a compatible FAT32 volume.
- The high-level SD API cannot distinguish every directory end condition from every media error. The filesystem checks the underlying card error after streamed directory reads and remounts after a failed root probe, but reliable hot swap during an operation is unsupported.
- Paths are limited to 64 normalized characters and FAT 8.3 components. There is no recursion, sorting, wildcard expansion, or atomic file move/overwrite.
- `mv` supports regular files only, refuses existing destinations, and uses non-atomic copy/verify/remove because Arduino SD 1.3.0 has no rename API.
- `nano` is limited to 1024 bytes of printable ASCII/LF text; its verified rewrite is not atomic and it has no search, clipboard, undo, or syntax features.
- `cat` is text-oriented, substitutes unsupported bytes, and stops after 4096 raw bytes.
- `sdinfo` lists only `/`; `sdtest` continues to own only `/SDTEST.TXT`.
- The standard SD `File` wrapper uses temporary heap handles internally as noted above.
- Touch, networking, multitasking, and general persistence are not implemented.
- `millis()` and `micros()` wraparound are handled with unsigned subtraction, but no long-duration wall clock is provided.

## Future TM4C123 mapping

```text
Arduino Serial       -> TM4C123 UART0 driver
LCDWIKI_SPI          -> custom ST7796S SPI driver
millis()/micros()    -> SysTick or hardware timers
watchdog reboot      -> Cortex-M SCB system reset
digitalRead/Write    -> TM4C123 GPIO register driver
Arduino SD/SPI       -> TM4C123 SPI SD/FAT diagnostic or storage layer
command parser       -> portable C shell module
history editor       -> portable fixed-buffer line editor
filesystem module    -> portable fixed-buffer path and file-operation layer
filesystem commands  -> portable argc/argv command handlers
text editor          -> portable fixed-buffer modal editor and viewport
```
