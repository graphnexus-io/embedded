#include "serial_input.h"

#include <Arduino.h>

namespace {

enum class ParserState : uint8_t {
    NORMAL,
    ESCAPE,
    CSI,
    SS3,
};

constexpr uint16_t ESCAPE_TIMEOUT_MS = 35U;

ParserState parser_state = ParserState::NORMAL;
uint32_t escape_started_ms = 0UL;
bool previous_was_cr = false;

InputEvent arrow_event(char character)
{
    switch (character) {
        case 'A': return InputEvent::UP;
        case 'B': return InputEvent::DOWN;
        case 'C': return InputEvent::RIGHT;
        case 'D': return InputEvent::LEFT;
        default: return InputEvent::NONE;
    }
}

InputEvent letter_event(char character)
{
    if (character >= 'a' && character <= 'z') {
        character = static_cast<char>(character - ('a' - 'A'));
    }
    switch (character) {
        case 'W': return InputEvent::UP;
        case 'S': return InputEvent::DOWN;
        case 'A': return InputEvent::LEFT;
        case 'D': return InputEvent::RIGHT;
        case 'P': return InputEvent::PAUSE;
        case 'R': return InputEvent::RESTART;
        case 'Q': return InputEvent::BACK;
        default: return InputEvent::NONE;
    }
}

}  // namespace

void serial_input_init()
{
    parser_state = ParserState::NORMAL;
    escape_started_ms = 0UL;
    previous_was_cr = false;
}

InputEvent serial_input_poll()
{
    if (parser_state == ParserState::ESCAPE && Serial.available() <= 0 &&
        static_cast<uint32_t>(millis() - escape_started_ms) >= ESCAPE_TIMEOUT_MS) {
        parser_state = ParserState::NORMAL;
        return InputEvent::BACK;
    }

    while (Serial.available() > 0) {
        const char character = static_cast<char>(Serial.read());

        if (parser_state == ParserState::ESCAPE) {
            if (character == '[') {
                parser_state = ParserState::CSI;
                continue;
            }
            if (character == 'O') {
                parser_state = ParserState::SS3;
                continue;
            }
            parser_state = ParserState::NORMAL;
            return InputEvent::BACK;
        }
        if (parser_state == ParserState::CSI || parser_state == ParserState::SS3) {
            parser_state = ParserState::NORMAL;
            const InputEvent event = arrow_event(character);
            if (event != InputEvent::NONE) {
                previous_was_cr = false;
                return event;
            }
            continue;
        }

        if (character == 0x1B) {
            parser_state = ParserState::ESCAPE;
            escape_started_ms = millis();
            continue;
        }
        if (character == '\r') {
            previous_was_cr = true;
            return InputEvent::SELECT;
        }
        if (character == '\n') {
            if (previous_was_cr) {
                previous_was_cr = false;
                continue;
            }
            return InputEvent::SELECT;
        }

        previous_was_cr = false;
        const InputEvent event = letter_event(character);
        if (event != InputEvent::NONE) {
            return event;
        }
    }
    return InputEvent::NONE;
}

