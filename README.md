# Embedded Projects

Code used for embedded-system project demonstrations, including Arduino, STM32, Tiva, C, C++, assembly, build experiments, and small Python tools or GUIs used during videos.

Each project lives in its own folder under a platform/category directory. For example:

```text
arduino/
  7_stepper_motor_encoder/
    sketch/
    GUI/
```

Every project folder should include a `README.md` with the hardware/software requirements, how to build or flash the firmware, how to run any helper tools, and any demo notes needed to reproduce the video.

## Repository Layout

- `arduino/` - Arduino-based demos and sketches.
- `stm32/` - STM32 firmware projects and experiments.
- `tiva/` - TI Tiva C / TM4C projects and experiments.

## Public Repo Notes

The `.gitignore` is set up to keep source code, documentation, and small configuration examples while excluding local build products, firmware binaries, Python virtual environments, editor files, logs, generated packages, and secrets.

If a project needs local settings, commit a safe example such as `.env.example` and keep the real `.env` file private.

## Suggested Project README Template

```markdown
# Project Name

Short description of what the project demonstrates.

## Hardware

- Board:
- Modules:
- Power:
- Wiring:

## Software

- Toolchain / IDE:
- Libraries:

## Build and Flash

Steps to compile and upload the firmware.

## Run

How to operate the demo and any PC-side tools.

## Notes

Calibration values, known limitations, or video-specific setup details.
```
