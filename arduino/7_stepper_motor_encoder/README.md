# Arduino Stepper Motor Encoder Controller

Arduino Mega-based demo for controlling a stepper motor through a step/dir driver while reading a quadrature encoder for position feedback. The project includes firmware for the controller and a Python Tkinter GUI for sending movement commands and viewing telemetry over serial.

## What It Demonstrates

- DM542-style stepper driver control with `STEP`, `DIR`, and `ENABLE` pins.
- Quadrature encoder reading on Arduino Mega interrupt pins 2 and 3.
- Position-error checking after a move completes.
- Serial command protocol for motion, tuning, status, and telemetry.
- Python desktop GUI for live control and monitoring.

## Hardware

- Arduino Mega 2560 or compatible board.
- Step/dir stepper driver such as a DM542.
- Stepper motor matched to the driver and power supply.
- Quadrature encoder, configured in the sketch as `1000` pulses per revolution and `4000` counts per revolution after x4 decoding.
- External motor power supply appropriate for the motor and driver.

Default firmware pins:

| Signal | Arduino Pin |
| --- | --- |
| STEP | 9 |
| DIR | 8 |
| ENABLE | 7 |
| Encoder A | 2 |
| Encoder B | 3 |

Check the driver wiring and voltage levels before powering the motor. The Arduino and driver signal ground should be common.

## Firmware

Firmware location:

```text
sketch/stepper.ino
```

Arduino library dependency:

- `AccelStepper`

Install with Arduino IDE:

1. Open Arduino IDE.
2. Go to `Tools > Manage Libraries`.
3. Search for `AccelStepper`.
4. Install the library by Mike McCauley.
5. Open `sketch/stepper.ino`.
6. Select `Arduino Mega or Mega 2560`.
7. Select the correct serial port.
8. Upload the sketch.

Important firmware values to verify before running:

- `motorPulsesPerRevolution` must match the driver's microstep DIP-switch setting.
- `encoderCountsPerRevolution` must match the encoder after quadrature decoding.
- `ENABLE_ACTIVE_LOW` must match the driver enable input.
- `INVERT_MOTOR_DIRECTION` or `SET_ENCODER_SIGN` may need adjustment if direction is reversed.

## Python GUI

GUI location:

```text
GUI/main.py
```

The GUI uses Python, Tkinter, and `pyserial`. Tkinter is included with many Python installs, but on Linux it is often packaged separately.

### Install on Linux

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install python3 python3-venv python3-tk
cd embedded/arduino/7_stepper_motor_encoder/GUI
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

If the serial port cannot be opened without root, add your user to the serial group and log out/in:

```bash
sudo usermod -a -G dialout "$USER"
```

### Install on Windows

1. Install Python 3 from python.org and enable `Add python.exe to PATH`.
2. Open PowerShell in `embedded/arduino/7_stepper_motor_encoder/GUI`.
3. Run:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

If PowerShell blocks activation, run:

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

### Run the GUI

Linux/macOS:

```bash
cd embedded/arduino/7_stepper_motor_encoder/GUI
source .venv/bin/activate
python main.py
```

Windows PowerShell:

```powershell
cd embedded\arduino\7_stepper_motor_encoder\GUI
.\.venv\Scripts\Activate.ps1
python main.py
```

In the GUI:

1. Select the Arduino serial port.
2. Click `Connect`.
3. Use the configuration fields to match the driver microsteps, encoder CPR, tolerance, acceleration, and telemetry interval.
4. Use `ENABLE`, `MOVE`, `STOP`, and `EMERGENCY STOP` for control.

## Serial Protocol

The firmware accepts newline-terminated ASCII commands at `115200` baud.

Common commands:

```text
MOVE <pulses> <CW|CCW> <rpm>
STOP
ESTOP
ENABLE
DISABLE
ZERO
STATUS
CONFIG
CLEAR_FAULT
CLEAR_ENCODER_ERRORS
SET_ACCEL <pulses_per_second_squared>
SET_TOLERANCE <encoder_counts>
SET_MOTOR_PPR <pulses_per_revolution>
SET_ENCODER_CPR <counts_per_revolution>
SET_ENCODER_SIGN <1|-1>
SET_MAX_RPM <rpm>
STREAM <ON|OFF>
SET_STREAM_MS <milliseconds>
HELP
```

Telemetry lines start with `DATA` and include controller state, commanded position, encoder position, expected encoder position, position error, speed, remaining distance, driver enable state, invalid encoder transitions, and current tolerance.

## Troubleshooting

- No serial port visible: check the USB cable, board selection, and OS serial permissions.
- GUI starts but cannot connect: close Arduino Serial Monitor or any other app using the port.
- Motor runs backward: change `INVERT_MOTOR_DIRECTION` in firmware or use the GUI/serial command `SET_ENCODER_SIGN -1` for encoder direction.
- Position faults after motion: verify motor microsteps, encoder CPR, wiring, driver current, acceleration, speed, and mechanical coupling.
- Encoder invalid transitions increase quickly: check encoder wiring, shielding, pullups, ground connection, and noise near motor wiring.
