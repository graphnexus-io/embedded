# Arduino Near-Infrared Detector Module

Arduino project for monitoring a near-infrared detector module with analog and digital outputs. The Arduino streams the raw infrared intensity and comparator state over serial, while a Python Tkinter GUI presents the live intensity and a high-contrast digital indicator.

## What It Demonstrates

- Reading the module's analog infrared-intensity output on Arduino pin `A0`.
- Reading the module's digital comparator output on pin `2`.
- Streaming both sensor signals over serial at `9600` baud.
- Displaying the live raw intensity and percentage of the ADC range.
- Showing a red circle when the digital signal is `HIGH` and a green circle when it is `LOW`.

## Hardware

- Arduino Uno, Nano, Mega, or another compatible board with an analog input.
- Near-infrared detector module with `AO` and `DO` outputs.
- USB data cable for programming and serial communication.
- Near-infrared source suitable for the detector, such as an IR remote control or IR LED.

Default wiring:

| Detector Signal | Arduino Pin |
| --- | --- |
| `AO` / analog signal | `A0` |
| `DO` / digital signal | `2` |
| `VCC` / `+` | Supply voltage specified by the module |
| `GND` / `-` | `GND` |

Check the detector module's supply-voltage and output-voltage specifications before wiring it. The input signal must remain within the safe range of the Arduino board. Many modules work at `5 V`, but this should not be assumed for every detector.

The onboard potentiometer, when fitted, adjusts the `DO` comparator threshold. It does not calibrate the analog output. Ambient sunlight and incandescent lighting contain infrared energy and can influence the reading, so test the module under lighting similar to its intended use.

## Firmware

Firmware location:

```text
sketch/sketch.ino
```

The sketch samples both detector outputs every `100 ms` and sends one telemetry record over USB serial. It does not require additional Arduino libraries.

Upload with Arduino IDE:

1. Open Arduino IDE.
2. Open `sketch/sketch.ino`.
3. Select the correct board from `Tools > Board`.
4. Select the correct serial port from `Tools > Port`.
5. Upload the sketch.
6. Close Serial Monitor before opening the Python GUI, because only one program can normally use the port at a time.

Default firmware settings:

| Setting | Value |
| --- | --- |
| Infrared analog input | `A0` |
| Digital comparator input | `2` |
| Serial baud rate | `9600` |
| Sample interval | `100 ms` |

## Serial Protocol

The firmware sends one newline-terminated ASCII record per sample:

```text
INFRARED_INTENSITY=487,DIGITAL_SIGNAL=1
```

- `INFRARED_INTENSITY` is the raw Arduino ADC reading from `0` to `1023`.
- `DIGITAL_SIGNAL` is the comparator output: `0` for `LOW` or `1` for `HIGH`.
- The firmware does not accept serial commands.

## Python GUI

GUI location:

```text
GUI/main.py
```

The dark GUI uses large values and high-contrast status colors. The circle and
the `Infrared source detected` display are red for `HIGH` and green for `LOW`.
This color mapping represents the electrical state; confirm how your particular
module maps that state to physical infrared detection.

Python dependency:

- `pyserial`

Tkinter is included with many Python installations. On Linux, it is often packaged separately.

### Install on Linux

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install python3 python3-venv python3-tk
cd embedded/arduino/10_near_infrared_detector_module/GUI
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
2. Open PowerShell in `embedded/arduino/10_near_infrared_detector_module/GUI`.
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
cd embedded/arduino/10_near_infrared_detector_module/GUI
source .venv/bin/activate
python main.py
```

Windows PowerShell:

```powershell
cd embedded\arduino\10_near_infrared_detector_module\GUI
.\.venv\Scripts\Activate.ps1
python main.py
```

In the GUI:

1. Click `Refresh` if the Arduino port is not listed.
2. Select the Arduino serial port.
3. Click `Connect`.
4. Aim an infrared source at the detector.
5. Watch the intensity gauge and digital signal indicator update live.

## Understanding the Readings

`INFRARED_INTENSITY` is shown as both a raw ADC count and a percentage of the full `0` to `1023` ADC range. The percentage is not a calibrated optical unit and should not be interpreted as radiant power or distance. Depending on the detector circuit, a stronger infrared source can make the raw value either rise or fall.

The digital output switches when the analog signal crosses the threshold selected by the module's potentiometer. Some comparator modules use active-low logic, meaning `LOW` indicates detection; others use active-high logic. The GUI uses these electrical-state colors:

| Digital Signal | Indicator Color |
| --- | --- |
| `HIGH (1)` | Red |
| `LOW (0)` | Green |

## Adjusting the Digital Threshold

1. Connect the GUI and place the infrared source at the desired detection distance.
2. Slowly turn the module potentiometer until the digital indicator changes state.
3. Move the source away and back again to verify repeatable switching.
4. Check whether `HIGH` or `LOW` corresponds to detection on your module.
5. Keep ambient lighting consistent when comparing results.

Near the switching point, the digital signal may alternate due to noise. Move farther from the threshold, adjust the potentiometer, or add software debouncing if the application requires a steadier output.

## Troubleshooting

- No serial port is visible: check the USB data cable, board connection, USB driver, and operating-system serial permissions.
- GUI cannot connect: close Arduino Serial Monitor and any other program using the port, then confirm the sketch uses `9600` baud.
- GUI reports unrecognized data: upload the included sketch and verify that records use `INFRARED_INTENSITY=...,DIGITAL_SIGNAL=...`.
- Intensity is stuck at `0` or `1023`: check detector power, ground, `AO` wiring, module voltage, and the Arduino analog-input pin.
- Intensity changes in the unexpected direction: some detector circuits invert the analog signal; compare relative changes instead of assuming a higher ADC count always means more infrared.
- Digital state never changes: adjust the potentiometer, check the `DO` connection to pin `2`, and try the infrared source closer to the sensor.
- Reading changes without the test source: shield the detector from sunlight, incandescent lamps, and other infrared sources.
- Remote control appears intermittent: many remotes transmit modulated pulses only while a button is pressed, so rapidly changing readings can be normal.
- Circle colors appear reversed for physical detection: the GUI maps electrical `HIGH` to red and `LOW` to green as specified; the detector module may use active-low logic.
