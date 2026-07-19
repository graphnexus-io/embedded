# Python GUI

Tkinter desktop GUI for the Arduino stepper motor encoder controller. It connects to the Arduino over serial at `115200` baud, sends motion/configuration commands, and displays live telemetry.

## Install

Linux:

```bash
sudo apt install python3 python3-venv python3-tk
python3 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

Windows PowerShell:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

## Run

```bash
python main.py
```

On Linux, add your user to the `dialout` group if the serial port is visible but cannot be opened:

```bash
sudo usermod -a -G dialout "$USER"
```

Log out and back in after changing group membership.
