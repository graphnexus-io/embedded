#include "joystick_input.h"

#include <Arduino.h>

#include "config.h"

namespace {

constexpr uint8_t CALIBRATION_SAMPLES = 16U;

int16_t center_x = 512;
int16_t center_y = 512;
InputEvent active_direction = InputEvent::NONE;
uint32_t last_sample_ms = 0UL;
uint32_t next_repeat_ms = 0UL;

bool raw_button_pressed = false;
bool stable_button_pressed = false;
bool long_press_sent = false;
uint32_t raw_button_changed_ms = 0UL;
uint32_t button_pressed_ms = 0UL;

int16_t absolute_value(int16_t value)
{
    return value < 0 ? static_cast<int16_t>(-value) : value;
}

InputEvent direction_from_deviation(
    int16_t deviation_x, int16_t deviation_y, uint16_t threshold)
{
    const int16_t magnitude_x = absolute_value(deviation_x);
    const int16_t magnitude_y = absolute_value(deviation_y);
    if (magnitude_x < static_cast<int16_t>(threshold) &&
        magnitude_y < static_cast<int16_t>(threshold)) {
        return InputEvent::NONE;
    }
    if (magnitude_x >= magnitude_y) {
        return deviation_x < 0 ? InputEvent::LEFT : InputEvent::RIGHT;
    }
    return deviation_y < 0 ? InputEvent::UP : InputEvent::DOWN;
}

InputEvent poll_button(uint32_t now_ms)
{
    const bool current_raw = digitalRead(JOYSTICK_BUTTON_PIN) == LOW;
    if (current_raw != raw_button_pressed) {
        raw_button_pressed = current_raw;
        raw_button_changed_ms = now_ms;
    }

    if (raw_button_pressed != stable_button_pressed &&
        static_cast<uint32_t>(now_ms - raw_button_changed_ms) >=
            JOYSTICK_BUTTON_DEBOUNCE_MS) {
        stable_button_pressed = raw_button_pressed;
        if (stable_button_pressed) {
            button_pressed_ms = now_ms;
            long_press_sent = false;
        } else if (!long_press_sent) {
            return InputEvent::SELECT;
        }
    }

    if (stable_button_pressed && !long_press_sent &&
        static_cast<uint32_t>(now_ms - button_pressed_ms) >=
            JOYSTICK_BUTTON_LONG_PRESS_MS) {
        long_press_sent = true;
        return InputEvent::BACK;
    }
    return InputEvent::NONE;
}

}  // namespace

void joystick_input_init()
{
    pinMode(JOYSTICK_BUTTON_PIN, INPUT_PULLUP);
    raw_button_pressed = digitalRead(JOYSTICK_BUTTON_PIN) == LOW;
    stable_button_pressed = raw_button_pressed;
    raw_button_changed_ms = millis();
    button_pressed_ms = raw_button_changed_ms;
    long_press_sent = false;

    uint32_t total_x = 0UL;
    uint32_t total_y = 0UL;
    for (uint8_t sample = 0U; sample < CALIBRATION_SAMPLES; ++sample) {
        total_x += static_cast<uint16_t>(analogRead(JOYSTICK_X_PIN));
        total_y += static_cast<uint16_t>(analogRead(JOYSTICK_Y_PIN));
    }
    center_x = static_cast<int16_t>(total_x / CALIBRATION_SAMPLES);
    center_y = static_cast<int16_t>(total_y / CALIBRATION_SAMPLES);
    active_direction = InputEvent::NONE;
    last_sample_ms = static_cast<uint32_t>(millis() - JOYSTICK_SAMPLE_INTERVAL_MS);
    next_repeat_ms = 0UL;
}

InputEvent joystick_input_poll(uint32_t now_ms)
{
    const InputEvent button_event = poll_button(now_ms);
    if (button_event != InputEvent::NONE) {
        return button_event;
    }
    if (static_cast<uint32_t>(now_ms - last_sample_ms) <
        JOYSTICK_SAMPLE_INTERVAL_MS) {
        return InputEvent::NONE;
    }
    last_sample_ms = now_ms;

    int16_t deviation_x = static_cast<int16_t>(
        analogRead(JOYSTICK_X_PIN) - center_x);
    int16_t deviation_y = static_cast<int16_t>(
        analogRead(JOYSTICK_Y_PIN) - center_y);
    if (JOYSTICK_REVERSE_X) {
        deviation_x = static_cast<int16_t>(-deviation_x);
    }
    if (JOYSTICK_REVERSE_Y) {
        deviation_y = static_cast<int16_t>(-deviation_y);
    }

    const InputEvent activated = direction_from_deviation(
        deviation_x, deviation_y, JOYSTICK_ACTIVATE_DELTA);
    if (active_direction == InputEvent::NONE) {
        if (activated != InputEvent::NONE) {
            active_direction = activated;
            next_repeat_ms = now_ms + JOYSTICK_INITIAL_REPEAT_MS;
            return active_direction;
        }
        return InputEvent::NONE;
    }

    const InputEvent held = direction_from_deviation(
        deviation_x, deviation_y, JOYSTICK_RELEASE_DELTA);
    if (held == InputEvent::NONE) {
        active_direction = InputEvent::NONE;
        return InputEvent::NONE;
    }
    if (activated != InputEvent::NONE && activated != active_direction) {
        active_direction = activated;
        next_repeat_ms = now_ms + JOYSTICK_INITIAL_REPEAT_MS;
        return active_direction;
    }
    if (static_cast<int32_t>(now_ms - next_repeat_ms) >= 0L) {
        next_repeat_ms = now_ms + JOYSTICK_REPEAT_MS;
        return active_direction;
    }
    return InputEvent::NONE;
}
