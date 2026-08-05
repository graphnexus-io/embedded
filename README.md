# Embedded projects

Firmware experiments and small desktop tools for Arduino and TI TM4C123
boards. Each project has its own directory and documents its circuit,
dependencies, and build procedure.

## Arduino

- [Stepper motor and encoder controller](arduino/7_stepper_motor_encoder/) —
  step/dir motion control, quadrature feedback, serial telemetry, and a Tkinter
  control panel.
- [Water-level monitor](arduino/8_water_level_sensor/) — analog sampling,
  calibration, and a live desktop display.
- [Temperature sensor and fan controller](arduino/9_temperature_sensor_module/)
  — analog/digital sensing, delayed fan switching, and two-point calibration.
- [Near-infrared detector](arduino/10_near_infrared_detector_module/) — analog
  intensity and comparator-state monitoring.
- [MiniOS embedded monitor](arduino/11_arduino_tft_spi_display/) — a serial/TFT
  command shell with SD-card filesystem commands and a small text editor.
- [Retro games](arduino/12_arduino_retro_games/) — seven fixed-memory games for
  an ST7796S TFT and joystick.
- [Sokoban 100](arduino/13_arduino_sokoban/) — a standalone Sokoban game with
  generated levels, EEPROM progress, undo, and joystick control.

## TI TM4C123

- [SN74HC74 flip-flop exploration](tiva/1_explore_SN74HC74_flip_flop/) — a
  bare-metal TM4C123GH6PM program that drives data and clock signals from GPIO.
- [Four-bit binary LED counter](tiva/2_binary_led/) — a bare-metal
  TM4C123GH6PM program that increments a PB0-PB3 LED display from the onboard
  SW1 button.

Project-specific READMEs contain the required toolchain and hardware details.
Build products, editor state, virtual environments, and local credentials are
excluded by [.gitignore](.gitignore).

## License

Repository code is released under the [MIT License](LICENSE). Vendored
third-party libraries retain their own license files.
