# Arduino Retro Games

This project is a small fixed-memory game system for the Arduino Mega 2560 and
a 480x320 ST7796S SPI TFT. Its dark surround, pale gray-green LCD panel, dark
text, and inset block graphics are inspired by monochrome handheld “Brick
Game” devices.

Version 0.5.0 implements Snake, Tetris, Breakout, Retro Racing, 2048, Sokoban,
and Lunar Lander. A table-driven,
scrolling selection menu lets additional games or application screens be
registered without redesigning the menu. This is a separate game firmware
project; it does not contain or reuse the MiniOS command shell.

## Hardware and wiring

- Arduino Mega 2560 / ATmega2560 at 16 MHz
- USB serial controls at 115200 baud
- 4-inch ST7796S TFT, 480x320 landscape
- LCDWIKI_GUI and LCDWIKI_SPI

```text
LCD_CS   -> D10
LCD_RST  -> D8
LCD_RS   -> D9
LCD_LED  -> D5
MOSI     -> D51
MISO     -> D50
SCK      -> D52
SD_CS    -> D4

Joystick SW  -> D2
Joystick URY -> A0
Joystick URX -> A1
```

D53 is held high as an output so the Mega hardware SPI controller remains in
master mode. LCDWIKI initializes D5 HIGH (255), and this firmware leaves it
steady with no PWM. The onboard MicroSD is not initialized; D4 remains
deselected. Touch is not initialized or used.

## Build, upload, and terminal

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

Upload, changing the port if necessary:

```sh
arduino-cli upload \
  --fqbn arduino:avr:mega:cpu=atmega2560 \
  --port /dev/ttyACM0 \
  --verify \
  ./sketch
```

Open the serial controller:

```sh
picocom -b 115200 /dev/ttyACM0
```

Exit picocom with Ctrl+A followed by Ctrl+X.

## Menu and controls

The boot state is the game menu, with Snake selected:

```text
RETRO GAMES

> 01 SNAKE
  02 TETRIS
  03 BREAKOUT
  04 RETRO RACING
  05 2048

PRESS/ENTER - START
STICK OR W/S - SELECT
1-5/7
```

The menu stores seven games in a `MenuEntry` table and displays
five rows at a time. Up/Down wraps the selection and scrolls the window; Enter
or a joystick-button tap opens the selected item. Because this release has no
screen above the main menu, Q/Esc or a hold there is a harmless back action.

The joystick is sampled non-blockingly and calibrated around its startup
position. Direction hysteresis prevents center noise, and held directions
repeat after a short delay. Its complete application-flow controls are:

- Stick: menu navigation or the active game's direction/action
- Short button tap: select, launch, hard-drop, pause, resume, or restart as
  appropriate to the current screen
- Hold about 0.7 seconds: pause an active game
- Continue holding to about 1.5 seconds: return/back to the menu
- On a pause screen: Up/right/tap resumes, Down restarts, Left/hold returns
- On a result screen: Up/right/tap restarts, Down/left/hold returns

Serial controls remain available in parallel with the joystick.

Snake controls accept uppercase or lowercase letters and ANSI arrow keys:

- W or Up: turn up
- S or Down: turn down
- A or Left: turn left
- D or Right: turn right
- P: pause or resume
- R: restart without resetting the board or display hardware
- Q or Esc: return to the menu

The firmware prints startup controls once. It prints the Snake controls when
entering the game, but does not emit per-frame serial messages.

Tetris controls:

- A or Left: move left
- D or Right: move right
- W or Up: rotate clockwise
- S or Down: soft drop one row
- Enter: hard drop and lock
- P: pause or resume
- R: restart
- Q or Esc: return to the menu

Tetris controls are printed once whenever Tetris is entered from the menu.

Breakout controls:

- A or Left: move the paddle left
- D or Right: move the paddle right
- Enter or Up: launch the ball at the start of a life
- P: pause or resume
- R: restart
- Q or Esc: return to the menu

Breakout controls are printed once whenever Breakout is entered from the menu.

Retro Racing controls:

- A or Left: steer one lane left
- D or Right: steer one lane right
- P: pause or resume
- R: restart
- Q or Esc: return to the menu

2048 and Sokoban both use WASD or the four arrow keys for grid movement. In
2048 each direction slides the complete board; in Sokoban it moves the player
and pushes a crate when the cell beyond it is clear.

Lunar Lander controls:

- W or Up: fire the main upward thruster
- A or Left: fire the left steering input, accelerating the lander left
- D or Right: fire the right steering input, accelerating the lander right
- P: pause or resume
- R: restart
- Q or Esc: return to the menu

Every accepted thrust event produces one bounded impulse. Holding the joystick
uses firmware repeat timing; holding a serial key uses the terminal's repeat.

## Backlight

LCDWIKI leaves D5 steadily HIGH (255). No `analogWrite()` call starts PWM,
brightness never changes during input, and no display settings are loaded or
saved.

## Snake rules

- Grid: 28 columns by 16 rows
- Cell size: 14x14 pixels
- Playfield: 392x224 pixels
- Initial length: four cells
- Initial direction: right
- Initial movement interval: 250 ms
- Speed increase: 10 ms per food, down to an 80 ms minimum
- Score: one point per food

The snake moves one cell per tick. Wall or body contact ends the game. Entering
the current tail cell is legal only on a non-growing tick because that cell is
being vacated. Immediate reversal is rejected. At most one accepted direction
change is queued before each movement tick, so a rapid two-key sequence cannot
turn through an intermediate direction and reverse the snake before it moves.

Food is never placed on the snake. Placement makes 64 bounded pseudo-random
attempts and then scans the 448 cells deterministically, guaranteeing that a
nearly full board cannot cause an infinite loop. Filling all cells produces a
win screen.

## Tetris rules

- Board: 10 columns by 20 rows
- Cell size: 12x12 pixels
- Seven tetrominoes represented by 4x4 bitmasks in program flash
- Shuffled seven-piece bag
- Next-piece preview
- Initial gravity interval: 600 ms
- One level per ten cleared lines
- Gravity accelerates by 50 ms per level to a 100 ms minimum

Pieces move and rotate only when every occupied mask cell remains inside the
board and clear of settled blocks. Rotation tries the current position followed
by small horizontal wall kicks. It is intentionally simpler than the full
modern Super Rotation System.

Soft-dropped rows score one point and hard-dropped rows score two. Clearing one,
two, three, or four lines awards 100, 300, 500, or 800 points multiplied by the
current level. Complete rows are compacted downward immediately. A piece that
locks partly above the visible board, or a new piece that cannot spawn, produces
game over. There is no blocking line-clear animation or lock delay.

## Breakout rules

- Brick field: 10 columns by 5 rows, for 50 bricks
- Lives: three
- Playfield: 376x244 pixels
- Paddle: 64x8 pixels
- Ball: 7x7 pixels, advanced every 28 ms
- Brick values: 50 points on the top row down to 10 on the bottom row

The ball begins attached to the paddle and is launched with Enter, a button tap,
or Up. Paddle position changes in bounded input steps. Paddle contact changes the
horizontal direction according to the impact area, while wall and brick contact
reflects the ball on the appropriate axis. Clearing every brick wins; allowing
the ball below the paddle consumes a life, and losing all three ends the game.

## Retro Racing rules

- Road: three lanes with vertically scrolling divider marks
- Traffic: four obstacle-car records recycled above the screen
- Initial update interval: 110 ms
- Maximum speed: 55 ms update interval
- Score: one point for each obstacle that passes the bottom of the road

The player steers between fixed lane centers while the road markings and
traffic move downward by eight pixels per update. Traffic is respawned with a
bounded lane-selection attempt, and the update interval gradually falls as the
score rises. Contact with any obstacle ends the run.

## 2048 rules

The game uses the standard 4x4 board. A move compacts each row or column toward
the selected edge, merges equal adjacent tiles once, and adds the merged value
to the score. A successful move creates a 2 tile with 90 percent probability or
a 4 tile with 10 percent probability. Reaching 2048 wins; filling the board
without an adjacent equal pair ends the game. Invalid moves do not create a new
tile.

## Sokoban rules

Sokoban now has five selectable flash-resident levels, from the original 12x9
map through a 22x13 test map. Tiles are 18x18 pixels; each map is dynamically
centered inside a 408x238 play region. The fixed mutable board supports at most
22 columns by 13 rows and consumes 286 SRAM bytes regardless of the selected
level. Level definitions keep explicit width and height values, with compile-
time source-length checks and runtime bounds/player/crate/target validation.

The player can walk onto floor or targets and can push, but never pull, a crate.
A push is accepted only when the cell beyond the crate is free. Target state is
preserved under the player and crates. Solid patterned masonry distinguishes
walls from thin outlined X-marked crates. Empty targets retain a square/dot
marker, while crates on targets gain an inner border and center mark. Placing
all crates on targets clears a level; a tap starts the next level.

## Lunar Lander rules

The lander uses integer fixed-point position and velocity with a 50 ms physics
step. Gravity adds one vertical speed unit per step. Main thrust costs three of
the initial 150 fuel units and subtracts six vertical speed units; a side thrust
costs one fuel and changes horizontal speed by two units. Speeds are bounded to
keep arithmetic and gameplay controlled.

The status line displays fuel (`F`), vertical speed (`V`), horizontal speed
(`H`), and pixel altitude (`A`). A safe landing requires the complete landing
gear to be over the marked pad, downward speed from 0 through 10, and absolute
horizontal speed no greater than 6. Touching terrain elsewhere or exceeding a
safe landing speed causes a crash.

## Application architecture

```text
sketch/sketch.ino       top-level menu and per-game play/pause/game-over states
sketch/config.h         pins, geometry, palette, timing, and fixed limits
sketch/display_hal.*    LCDWIKI initialization and drawing primitives
sketch/serial_input.*   non-blocking raw-byte to InputEvent parser
sketch/joystick_input.* calibrated, debounced InputEvent source with repeats
sketch/game_menu.*      table-driven menu selection and rendering
sketch/snake_game.*     Snake storage, rules, timing, and incremental drawing
sketch/tetris_game.*    Tetris board, pieces, scoring, gravity, and rendering
sketch/breakout_game.*  Breakout bricks, ball physics, scoring, and rendering
sketch/racing_game.*    scrolling road, traffic recycling, collision, and score
sketch/game_2048.*      tile sliding, merging, spawning, and board rendering
sketch/sokoban_game.*   flash levels, crate rules, target state, and rendering
sketch/lunar_lander.*   fixed-point flight physics, fuel, terrain, and landing
```

`loop()` polls the serial parser and joystick once, then dispatches the current
application state. Serial has priority only when both sources produce an event
on the same pass. There are no nested game loops or blocking input waits. Pause
records no movement; resume resets the last-move timestamp and redraws the
playfield, preventing a queued timing jump.

The serial parser is a finite-state machine with normal, Escape, CSI, and SS3
states. It consumes `ESC [ A/B/C/D` and common `ESC O A/B/C/D` arrow sequences
one byte at a time. A standalone Esc becomes `BACK` after a short non-blocking
timeout. CR, LF, and CRLF generate one `SELECT` event.

## Fixed-memory design

Snake cells are two bytes each. A fixed 448-entry array therefore reserves 896
bytes and can represent a completely full board. Cells are arranged as a ring
from tail to head. A normal tick advances the tail index and appends the new
head; growth appends without advancing the tail. No body array is shifted.

Tetris reserves a fixed 20x10 byte board (200 bytes), a seven-byte shuffled
bag, and a few scalar fields for its active and next pieces. Its 28 rotation
masks consume 56 bytes of program flash rather than SRAM. Rows are compacted
in place with bounded copies only when lines are cleared.

Breakout reserves one Boolean flag for each of its 50 bricks and uses only
fixed-width scalar state for the paddle, ball, score, lives, and timing. It has
no object list, heap storage, or off-screen playfield image.

Racing stores four small obstacle records. 2048 stores sixteen 16-bit tile
values. Sokoban stores a 286-byte maximum mutable board while all five source
maps remain in program flash. Lunar Lander stores only fixed-width position,
velocity, fuel, and timer fields. All game storage exists at compile time.

The project uses no Arduino `String`, explicit dynamic allocation, recursion,
exceptions, RTOS, or framebuffer. Small numeric and menu strings use bounded
local character arrays. The normal game loop has no `delay()`.

## Rendering optimization

Menu entry, game start, restart, resume, and overlays may redraw their complete
static view because they are infrequent transitions. Normal movement never
clears the screen or redraws the complete snake. Each tick:

1. converts the old head to the body appearance;
2. erases the old tail only when the snake did not grow;
3. draws the new head;
4. redraws score and food only after eating.

All blocks use LCDWIKI filled-rectangle operations rather than per-pixel loops.

Tetris normally erases only the four old active-piece cells and draws the four
new cells. Locking converts those cells to the settled appearance. The complete
10x20 board is redrawn only after row compaction, restart, or resume; score,
lines, level, and next-piece regions are updated independently.

Breakout erases and redraws only the moving ball on each timed step. Paddle
movement updates only the old and new paddle regions; a hit updates one brick
and the score. The complete view is drawn only at start, restart, or resume, so
ordinary play does not flash or incur a full-screen redraw.

Racing updates only the old/new divider marks, visible traffic cars, player car,
and score instead of scrolling or clearing the whole road bitmap. 2048 redraws
its sixteen cells only after a valid move. Sokoban redraws the vacated player
cell, new player cell, optional pushed-crate destination, and only the counters
that changed. Lunar Lander erases and redraws only its sprite each physics step
and refreshes flight statistics at most every 200 ms unless thrust changes fuel.

## Known limitations

- Seven games are implemented, but each is deliberately a compact first
  prototype rather than a complete commercial-game clone.
- The joystick is a digital-direction abstraction; Racing and Breakout do not
  use proportional analog steering.
- The menu's Q/Esc back action has no parent screen in this release.
- Food placement uses a simple analog/microsecond seed and is not
  cryptographically random.
- There is no saved high score, sound, difficulty menu, attract mode, or
  multiplayer support.
- Tetris has no hold piece, ghost piece, lock delay, line-clear animation, or
  full Super Rotation System kicks.
- Breakout uses discrete paddle movement and simple axis-aligned
  collision. It has no key-state tracking, levels, power-ups, or multi-brick
  collision resolution in one ball step.
- Racing uses three discrete lanes and has no curves, braking, or continuous
  steering model.
- 2048 has no undo, saved best score, or continue-after-2048 mode.
- Sokoban has five demonstration levels but no undo or deadlock detection.
- Lunar Lander uses stepped terrain, discrete thrust impulses, and a
  simple fixed-point physics model rather than continuous rigid-body dynamics.
- LCDWIKI performs blocking SPI drawing calls; active game logic remains
  non-blocking, but a display primitive occupies the CPU while it transfers.
- Hardware play testing is still required to validate long-run behavior and
  the exact appearance on the connected panel.

## Future TM4C123 port

```text
LCDWIKI display HAL  -> custom TM4C123 SPI/ST7796S driver
Arduino Serial       -> TM4C123 UART driver
Arduino SD/File      -> TM4C123 FAT/block-storage adapter
analogRead joystick  -> TM4C123 ADC and GPIO input driver
steady D5 HIGH       -> TM4C123 GPIO backlight enable
millis()             -> SysTick or hardware timer
analogRead/micros    -> timer/ADC-derived game seed
menu/state machine   -> portable C application module
Snake rules/ring     -> portable fixed-memory C game module
Tetris rules/board   -> portable fixed-memory C game module
Breakout rules/state -> portable fixed-memory C game module
Racing objects/road  -> portable fixed-memory C game module
2048 tile rules      -> portable fixed-memory C game module
Sokoban grid/rules   -> portable fixed-memory C game module
Lander physics       -> portable fixed-point C game module
```

The display wrapper and `InputEvent` boundary keep Arduino-specific APIs out of
the menu and most game rules.
