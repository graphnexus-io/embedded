# Arduino Temperature Sensor and Fan Controller

Arduino demo for monitoring an analog/digital thermistor module and using its adjustable digital threshold to control a fan signal. The Arduino streams both sensor outputs over serial, while a Python Tkinter GUI shows the raw reading, estimated voltage, calibrated temperature, and threshold state.

## What It Demonstrates

- Reading a thermistor module's analog output on Arduino pin `A0`.
- Reading the module comparator's digital threshold output on pin `2`.
- Mirroring the threshold state to a fan-control output on pin `3` after a `1 s` state-change delay.
- Streaming sensor telemetry over serial at `9600` baud.
- Applying a two-point temperature calibration in a Python Tkinter GUI.

## Hardware

- Arduino Uno, Nano, Mega, or another compatible 5 V board.
- Analog/digital thermistor module with `AO` and `DO` outputs, such as a KY-028-compatible module.
- USB cable for programming and serial communication.
- A trusted thermometer for calibration.
- Optional fan and a suitable transistor, logic-level MOSFET, relay module, or motor driver.
- An external fan power supply if required by the fan.

Default wiring:

| Module / Driver Signal | Arduino Pin |
| --- | --- |
| Sensor `AO` / analog signal | `A0` |
| Sensor `DO` / digital signal | `2` |
| Sensor `VCC` / `+` | `5V` |
| Sensor `GND` / `-` | `GND` |
| Fan driver control input | `3` |
| Fan driver ground | `GND` |

Do not connect a fan motor directly to Arduino pin `3`. An Arduino GPIO pin cannot safely supply typical fan current, and an inductive load needs an appropriate driver and protection. Connect the Arduino ground, sensor ground, and fan-driver control ground together. Power the fan from a supply rated for it; do not assume the Arduino `5V` pin can supply it.

The module potentiometer adjusts only the `DO` switching threshold. It does not calibrate the analog output or directly represent a temperature in degrees. Depending on the module design and wiring, `DO` may be `HIGH` above or below the selected threshold; verify its behavior before using it to control real equipment.

## Firmware

Firmware location:

```text
sketch/sketch/sketch.ino
```

The sketch samples the module every `100 ms`. It prints the current analog and digital readings, then mirrors a stable digital state to pin `3`. A requested state must remain unchanged for `1000 ms` before the output changes, which helps prevent rapid switching near the comparator threshold.

Upload with Arduino IDE:

1. Open Arduino IDE.
2. Open `sketch/sketch/sketch.ino`.
3. Select the correct board from `Tools > Board`.
4. Select the correct serial port from `Tools > Port`.
5. Upload the sketch.
6. Close Serial Monitor before opening the Python GUI, because only one program can normally use the serial port at a time.

No additional Arduino libraries are required.

Default firmware settings:

| Setting | Value |
| --- | --- |
| Analog input | `A0` |
| Digital input | `2` |
| Fan-driver output | `3` |
| Serial baud rate | `9600` |
| Sample interval | `100 ms` |
| Output switching delay | `1000 ms` |

## Serial Protocol

The firmware sends one newline-terminated ASCII record per sample:

```text
ANALOG_SIGNAL=487,DIGITAL_SIGNAL=1
```

- `ANALOG_SIGNAL` is the Arduino ADC reading from `0` to `1023`.
- `DIGITAL_SIGNAL` is the comparator output and is either `0` (`LOW`) or `1` (`HIGH`).

The firmware does not accept serial commands.

## Python GUI

GUI location:

```text
GUI/main.py
```

Python dependency:

- `pyserial`

Tkinter is included with many Python installations. On Linux, it is often packaged separately.

### Install on Linux

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install python3 python3-venv python3-tk
cd embedded/arduino/9_temperature_sensor_module/GUI
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

If the serial port is visible but cannot be opened, add your user to the serial group and log out/in:

```bash
sudo usermod -a -G dialout "$USER"
```

### Install on Windows

1. Install Python 3 from python.org and enable `Add python.exe to PATH`.
2. Open PowerShell in `embedded/arduino/9_temperature_sensor_module/GUI`.
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

## Run

Linux/macOS:

```bash
cd embedded/arduino/9_temperature_sensor_module/GUI
source .venv/bin/activate
python main.py
```

Windows PowerShell:

```powershell
cd embedded\arduino\9_temperature_sensor_module\GUI
.\.venv\Scripts\Activate.ps1
python main.py
```

In the GUI:

1. Click `Refresh` if the Arduino port is not listed.
2. Select the Arduino serial port.
3. Click `Connect`.
4. Enter two measured temperature/ADC calibration pairs.
5. Watch the estimated temperature, analog value, voltage, digital threshold state, and fan request update live.

The displayed fan status follows the current `DIGITAL_SIGNAL`. The physical output on pin `3` can lag that indication by up to one second because the switching delay is applied in the firmware.

## Temperature Calibration

The analog value is not converted to a trustworthy temperature automatically. The GUI starts with placeholder values:

| Calibration Point | Temperature | ADC Value |
| --- | --- | --- |
| Point 1 | `20.0 °C` | `500` |
| Point 2 | `40.0 °C` | `350` |

Replace both points with measurements from your own sensor:

1. Place the thermistor beside a trusted thermometer and allow both readings to stabilize.
2. Record the reference temperature and the GUI's analog value as point 1.
3. Repeat at a second temperature near the intended operating range for point 2.
4. Enter both pairs in the GUI.
5. Do not adjust the module potentiometer after calibration if you also want the digital threshold to remain consistent.

The GUI linearly interpolates and extrapolates between the two points. Thermistors are nonlinear, so this is an approximation that works best over a limited temperature range. For wider-range or higher-accuracy measurements, use the thermistor's resistance/Beta or Steinhart-Hart parameters and a measured reference voltage.

The voltage display assumes a `5.0 V` ADC reference and a 10-bit ADC (`0` to `1023`). Change `VREF` or `ADC_MAX` in `GUI/main.py` if the board uses different values. The voltage shown is also approximate unless the actual reference voltage is measured.

## Adjusting the Digital Threshold

1. Open the GUI and connect to the Arduino.
2. Bring the sensor to the temperature where the fan should switch.
3. Slowly turn the module potentiometer until `Digital input` changes state.
4. Move slightly above and below the threshold to confirm the direction and switching behavior.
5. If the fan logic is reversed, invert the state in firmware or use an inverting driver stage.

The module comparator may chatter when the temperature is very close to its threshold. The firmware's one-second state-change delay reduces rapid output changes but does not add a separate on/off temperature hysteresis band.

## Troubleshooting

- No serial port is visible: check the USB data cable, board connection, USB driver, and operating-system serial permissions.
- GUI cannot connect: close Arduino Serial Monitor and any other program using the same port, then verify that `9600` baud is selected by the firmware.
- GUI reports unrecognized lines: confirm that the uploaded sketch emits the documented `ANALOG_SIGNAL=...,DIGITAL_SIGNAL=...` format.
- Analog reading is stuck at `0` or `1023`: check `AO`, power, and ground wiring; also verify that the sensor output voltage is safe for the board.
- Temperature moves in the wrong direction: thermistor modules commonly produce decreasing ADC readings as temperature rises. Recheck the two calibration pairs instead of assuming the direction.
- Temperature is inaccurate: replace the placeholder calibration values, use two sufficiently separated stable temperatures, and keep measurements near the calibrated range.
- Digital state never changes: adjust the module potentiometer and verify the `DO` connection to pin `2`.
- Fan logic is reversed: confirm whether the module asserts `DO` high or low at the selected temperature and invert the firmware logic if needed.
- Fan does not run: verify the driver, fan supply, shared ground, fan polarity, and pin `3` control signal. Never test by powering the fan directly from a GPIO pin.
- Output switches late: the `1000 ms` delay is intentional; change `SWITCH_DELAY_MS` in the sketch if the application needs a different delay.
