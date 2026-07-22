# Arduino Water Level Sensor Monitor

Arduino demo for reading an analog water level sensor and displaying the measured level in a Python Tkinter GUI. The Arduino prints raw analog readings over serial, and the desktop GUI converts those readings into a calibrated percentage with a simple tank visualization.

## What It Demonstrates

- Reading an analog water level sensor on Arduino pin `A0`.
- Streaming sensor readings over serial at `9600` baud.
- Converting raw ADC values to approximate water-level percentages.
- Displaying live sensor data in a Python Tkinter desktop GUI.

## Hardware

- Arduino Uno, Nano, Mega, or another compatible board with analog input `A0`.
- Analog water level sensor.
- USB cable for serial communication.
- Test container or controlled water source for calibration.

Default wiring:

| Sensor Signal | Arduino Pin |
| --- | --- |
| Signal / S / AO | A0 |
| VCC | 5V or 3.3V, according to the sensor module |
| GND | GND |

Check the sensor module voltage rating before connecting power. Many low-cost water level sensors are resistive and should not be left powered in water for long periods because corrosion can affect readings.

## Firmware

Firmware location:

```text
sketch/sketch.ino
```

The sketch reads `A0`, prints the raw ADC value, and waits `50 ms` before the next reading.

Upload with Arduino IDE:

1. Open Arduino IDE.
2. Open `sketch/sketch.ino`.
3. Select the correct board from `Tools > Board`.
4. Select the correct serial port from `Tools > Port`.
5. Upload the sketch.
6. Close Serial Monitor before opening the Python GUI, because only one program can normally use the serial port at a time.

Serial settings:

```text
Baud rate: 9600
Line format: one numeric raw sensor value per line
```

## Python GUI

GUI location:

```text
GUI/main.py
```

Python dependency:

- `pyserial`

Tkinter is part of many Python installations. On Linux, it is often packaged separately.

### Install on Linux

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install python3 python3-venv python3-tk
cd embedded/arduino/8_water_level_sensor/GUI
python3 -m venv .env
source .env/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

If the serial port is visible but cannot be opened, add your user to the serial group and log out/in:

```bash
sudo usermod -a -G dialout "$USER"
```

### Install on Windows

1. Install Python 3 from python.org and enable `Add python.exe to PATH`.
2. Open PowerShell in `embedded/arduino/8_water_level_sensor/GUI`.
3. Run:

```powershell
py -m venv .env
.\.env\Scripts\Activate.ps1
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
cd embedded/arduino/8_water_level_sensor/GUI
source .env/bin/activate
python main.py
```

Windows PowerShell:

```powershell
cd embedded\arduino\8_water_level_sensor\GUI
.\.env\Scripts\Activate.ps1
python main.py
```

In the GUI:

1. Click `Refresh` if the Arduino port is not listed.
2. Select the Arduino serial port.
3. Click `Connect`.
4. Watch the raw sensor value and calculated percentage update live.

## Calibration

The GUI currently uses these calibration points:

| Raw Sensor Value | Water Level |
| --- | --- |
| 0 | 0% |
| 480 | 25% |
| 555 | 50% |
| 565 | 80% |
| 580 | 100% |

The percentage is linearly interpolated between neighboring calibration points. If your sensor, container, or supply voltage is different, update `CALIBRATION_POINTS` in `GUI/main.py`.

## Troubleshooting

- No serial port visible: check the USB cable, board selection, and Arduino driver.
- GUI cannot connect: close Arduino Serial Monitor or any other program using the port.
- Readings are stuck at zero: check sensor power, ground, and the signal wire to `A0`.
- Readings jump around: use stable wiring, avoid touching the sensor contacts, and average readings in firmware if needed.
- Percentage is inaccurate: recalibrate using your own water heights and update `CALIBRATION_POINTS`.
