#include <Arduino.h>

#include "config.h"
#include "breakout_game.h"
#include "display_hal.h"
#include "game_2048.h"
#include "game_menu.h"
#include "lunar_lander.h"
#include "joystick_input.h"
#include "racing_game.h"
#include "serial_input.h"
#include "snake_game.h"
#include "sokoban_game.h"
#include "tetris_game.h"

enum class AppState : uint8_t {
    MENU,
    SNAKE_PLAYING,
    SNAKE_PAUSED,
    SNAKE_GAME_OVER,
    TETRIS_PLAYING,
    TETRIS_PAUSED,
    TETRIS_GAME_OVER,
    BREAKOUT_PLAYING,
    BREAKOUT_PAUSED,
    BREAKOUT_GAME_OVER,
    RACING_PLAYING,
    RACING_PAUSED,
    RACING_GAME_OVER,
    GAME_2048_PLAYING,
    GAME_2048_PAUSED,
    GAME_2048_OVER,
    SOKOBAN_LEVEL_SELECT,
    SOKOBAN_PLAYING,
    SOKOBAN_PAUSED,
    SOKOBAN_WON,
    LANDER_PLAYING,
    LANDER_PAUSED,
    LANDER_OVER,
};

namespace {

AppState app_state = AppState::MENU;

bool paused_menu_event(InputEvent event)
{
    return event == InputEvent::BACK || event == InputEvent::LEFT;
}

bool paused_restart_event(InputEvent event)
{
    return event == InputEvent::RESTART || event == InputEvent::DOWN;
}

bool paused_resume_event(InputEvent event)
{
    return event == InputEvent::PAUSE || event == InputEvent::SELECT ||
           event == InputEvent::UP || event == InputEvent::RIGHT;
}

bool result_menu_event(InputEvent event)
{
    return event == InputEvent::BACK || event == InputEvent::DOWN ||
           event == InputEvent::LEFT;
}

bool result_restart_event(InputEvent event)
{
    return event == InputEvent::RESTART || event == InputEvent::SELECT ||
           event == InputEvent::UP || event == InputEvent::RIGHT;
}

void print_startup_controls()
{
    Serial.println(F("Retro Games"));
    Serial.println(F("WASD or arrow keys"));
    Serial.println(F("Enter: select"));
    Serial.println(F("P: pause"));
    Serial.println(F("R: restart"));
    Serial.println(F("Q: menu/back"));
    Serial.println(F("Joystick: stick moves; tap selects; hold pauses, then returns"));
}

void print_snake_controls()
{
    Serial.println(F("Snake controls:"));
    Serial.println(F("WASD/arrows move, P pauses, R restarts, Q/Esc returns to menu"));
}

void print_tetris_controls()
{
    Serial.println(F("Tetris controls:"));
    Serial.println(F("Left/Right move, Up rotates, Down soft-drops, Enter hard-drops"));
    Serial.println(F("P pauses, R restarts, Q/Esc returns to menu"));
}

void print_breakout_controls()
{
    Serial.println(F("Breakout controls:"));
    Serial.println(F("Left/Right or A/D moves, Enter or Up launches"));
    Serial.println(F("P pauses, R restarts, Q/Esc returns to menu"));
}

void print_racing_controls()
{
    Serial.println(F("Retro Racing controls:"));
    Serial.println(F("Left/Right or A/D steers between lanes"));
    Serial.println(F("P pauses, R restarts, Q/Esc returns to menu"));
}

void print_2048_controls()
{
    Serial.println(F("2048 controls:"));
    Serial.println(F("WASD or arrows slides every tile"));
    Serial.println(F("P pauses, R restarts, Q/Esc returns to menu"));
}

void print_sokoban_controls()
{
    Serial.println(F("Sokoban controls:"));
    Serial.println(F("WASD or arrows moves and pushes crates"));
    Serial.println(F("P pauses, R restarts, Q/Esc returns to menu"));
}

void print_lander_controls()
{
    Serial.println(F("Lunar Lander controls:"));
    Serial.println(F("Up/W main thrust, Left/Right or A/D side thrust"));
    Serial.println(F("P pauses, R restarts, Q/Esc returns to menu"));
}

void enter_menu()
{
    app_state = AppState::MENU;
    game_menu_enter();
}

void start_snake()
{
    print_snake_controls();
    app_state = AppState::SNAKE_PLAYING;
    snake_game_start(millis());
}

void start_tetris()
{
    print_tetris_controls();
    app_state = AppState::TETRIS_PLAYING;
    tetris_game_start(millis());
}

void start_breakout()
{
    print_breakout_controls();
    app_state = AppState::BREAKOUT_PLAYING;
    breakout_game_start(millis());
}

void start_racing()
{
    print_racing_controls();
    app_state = AppState::RACING_PLAYING;
    racing_game_start(millis());
}

void start_2048()
{
    print_2048_controls();
    app_state = AppState::GAME_2048_PLAYING;
    game_2048_start();
}

void start_sokoban()
{
    print_sokoban_controls();
    app_state = AppState::SOKOBAN_LEVEL_SELECT;
    sokoban_level_select_enter();
}

void start_lander()
{
    print_lander_controls();
    app_state = AppState::LANDER_PLAYING;
    lunar_lander_start(millis());
}

void update_menu(InputEvent event)
{
    const GameId selected = game_menu_handle_input(event);
    switch (selected) {
        case GameId::SNAKE: start_snake(); break;
        case GameId::TETRIS: start_tetris(); break;
        case GameId::BREAKOUT: start_breakout(); break;
        case GameId::RACING: start_racing(); break;
        case GameId::GAME_2048: start_2048(); break;
        case GameId::SOKOBAN: start_sokoban(); break;
        case GameId::LUNAR_LANDER: start_lander(); break;
        case GameId::NONE: break;
    }
}

void update_sokoban_level_select(InputEvent event)
{
    if (event == InputEvent::BACK) {
        enter_menu();
    } else if (sokoban_level_select_update(event)) {
        app_state = AppState::SOKOBAN_PLAYING;
        sokoban_game_start_selected();
    }
}

void update_lander_playing(InputEvent event, uint32_t now_ms)
{
    if (event == InputEvent::BACK) {
        enter_menu();
        return;
    }
    if (event == InputEvent::PAUSE || event == InputEvent::SELECT) {
        app_state = AppState::LANDER_PAUSED;
        lunar_lander_show_paused();
        return;
    }
    if (event == InputEvent::RESTART) {
        lunar_lander_start(now_ms);
        return;
    }

    const LanderUpdateResult result = lunar_lander_update(event, now_ms);
    if (result != LanderUpdateResult::RUNNING) {
        app_state = AppState::LANDER_OVER;
        lunar_lander_show_result(result == LanderUpdateResult::LANDED);
    }
}

void update_lander_paused(InputEvent event, uint32_t now_ms)
{
    if (paused_menu_event(event)) {
        enter_menu();
    } else if (paused_restart_event(event)) {
        app_state = AppState::LANDER_PLAYING;
        lunar_lander_start(now_ms);
    } else if (paused_resume_event(event)) {
        app_state = AppState::LANDER_PLAYING;
        lunar_lander_resume(now_ms);
    }
}

void update_lander_over(InputEvent event, uint32_t now_ms)
{
    if (result_menu_event(event)) {
        enter_menu();
    } else if (result_restart_event(event)) {
        app_state = AppState::LANDER_PLAYING;
        lunar_lander_start(now_ms);
    }
}

void update_sokoban_playing(InputEvent event)
{
    if (event == InputEvent::BACK) {
        enter_menu();
        return;
    }
    if (event == InputEvent::PAUSE || event == InputEvent::SELECT) {
        app_state = AppState::SOKOBAN_PAUSED;
        sokoban_game_show_paused();
        return;
    }
    if (event == InputEvent::RESTART) {
        sokoban_game_start_selected();
        return;
    }
    if (sokoban_game_update(event) == SokobanUpdateResult::WON) {
        app_state = AppState::SOKOBAN_WON;
        sokoban_game_show_won();
    }
}

void update_sokoban_paused(InputEvent event)
{
    if (paused_menu_event(event)) {
        enter_menu();
    } else if (paused_restart_event(event)) {
        app_state = AppState::SOKOBAN_PLAYING;
        sokoban_game_start_selected();
    } else if (paused_resume_event(event)) {
        app_state = AppState::SOKOBAN_PLAYING;
        sokoban_game_resume();
    }
}

void update_sokoban_won(InputEvent event)
{
    if (result_menu_event(event)) {
        enter_menu();
    } else if (result_restart_event(event)) {
        app_state = AppState::SOKOBAN_PLAYING;
        if (event == InputEvent::RESTART) {
            sokoban_game_start_selected();
        } else {
            sokoban_game_start_next();
        }
    }
}

void update_2048_playing(InputEvent event)
{
    if (event == InputEvent::BACK) {
        enter_menu();
        return;
    }
    if (event == InputEvent::PAUSE || event == InputEvent::SELECT) {
        app_state = AppState::GAME_2048_PAUSED;
        game_2048_show_paused();
        return;
    }
    if (event == InputEvent::RESTART) {
        game_2048_start();
        return;
    }

    const Game2048UpdateResult result = game_2048_update(event);
    if (result != Game2048UpdateResult::RUNNING) {
        app_state = AppState::GAME_2048_OVER;
        game_2048_show_result(result == Game2048UpdateResult::WON);
    }
}

void update_2048_paused(InputEvent event)
{
    if (paused_menu_event(event)) {
        enter_menu();
    } else if (paused_restart_event(event)) {
        app_state = AppState::GAME_2048_PLAYING;
        game_2048_start();
    } else if (paused_resume_event(event)) {
        app_state = AppState::GAME_2048_PLAYING;
        game_2048_resume();
    }
}

void update_2048_over(InputEvent event)
{
    if (result_menu_event(event)) {
        enter_menu();
    } else if (result_restart_event(event)) {
        app_state = AppState::GAME_2048_PLAYING;
        game_2048_start();
    }
}

void update_racing_playing(InputEvent event, uint32_t now_ms)
{
    if (event == InputEvent::BACK) {
        enter_menu();
        return;
    }
    if (event == InputEvent::PAUSE || event == InputEvent::SELECT) {
        app_state = AppState::RACING_PAUSED;
        racing_game_show_paused();
        return;
    }
    if (event == InputEvent::RESTART) {
        racing_game_start(now_ms);
        return;
    }
    if (racing_game_update(event, now_ms) == RacingUpdateResult::GAME_OVER) {
        app_state = AppState::RACING_GAME_OVER;
        racing_game_show_game_over();
    }
}

void update_racing_paused(InputEvent event, uint32_t now_ms)
{
    if (paused_menu_event(event)) {
        enter_menu();
    } else if (paused_restart_event(event)) {
        app_state = AppState::RACING_PLAYING;
        racing_game_start(now_ms);
    } else if (paused_resume_event(event)) {
        app_state = AppState::RACING_PLAYING;
        racing_game_resume(now_ms);
    }
}

void update_racing_game_over(InputEvent event, uint32_t now_ms)
{
    if (result_menu_event(event)) {
        enter_menu();
    } else if (result_restart_event(event)) {
        app_state = AppState::RACING_PLAYING;
        racing_game_start(now_ms);
    }
}

void update_breakout_playing(InputEvent event, uint32_t now_ms)
{
    if (event == InputEvent::BACK) {
        enter_menu();
        return;
    }
    if (event == InputEvent::PAUSE) {
        app_state = AppState::BREAKOUT_PAUSED;
        breakout_game_show_paused();
        return;
    }
    if (event == InputEvent::RESTART) {
        breakout_game_start(now_ms);
        return;
    }

    const BreakoutUpdateResult result = breakout_game_update(event, now_ms);
    if (result != BreakoutUpdateResult::RUNNING) {
        app_state = AppState::BREAKOUT_GAME_OVER;
        breakout_game_show_result(result == BreakoutUpdateResult::WON);
    }
}

void update_breakout_paused(InputEvent event, uint32_t now_ms)
{
    if (paused_menu_event(event)) {
        enter_menu();
    } else if (paused_restart_event(event)) {
        app_state = AppState::BREAKOUT_PLAYING;
        breakout_game_start(now_ms);
    } else if (paused_resume_event(event)) {
        app_state = AppState::BREAKOUT_PLAYING;
        breakout_game_resume(now_ms);
    }
}

void update_breakout_game_over(InputEvent event, uint32_t now_ms)
{
    if (result_menu_event(event)) {
        enter_menu();
    } else if (result_restart_event(event)) {
        app_state = AppState::BREAKOUT_PLAYING;
        breakout_game_start(now_ms);
    }
}

void update_tetris_playing(InputEvent event, uint32_t now_ms)
{
    if (event == InputEvent::BACK) {
        enter_menu();
        return;
    }
    if (event == InputEvent::PAUSE) {
        app_state = AppState::TETRIS_PAUSED;
        tetris_game_show_paused();
        return;
    }
    if (event == InputEvent::RESTART) {
        tetris_game_start(now_ms);
        return;
    }

    if (tetris_game_update(event, now_ms) == TetrisUpdateResult::GAME_OVER) {
        app_state = AppState::TETRIS_GAME_OVER;
        tetris_game_show_game_over();
    }
}

void update_tetris_paused(InputEvent event, uint32_t now_ms)
{
    if (paused_menu_event(event)) {
        enter_menu();
    } else if (paused_restart_event(event)) {
        app_state = AppState::TETRIS_PLAYING;
        tetris_game_start(now_ms);
    } else if (paused_resume_event(event)) {
        app_state = AppState::TETRIS_PLAYING;
        tetris_game_resume(now_ms);
    }
}

void update_tetris_game_over(InputEvent event, uint32_t now_ms)
{
    if (result_menu_event(event)) {
        enter_menu();
    } else if (result_restart_event(event)) {
        app_state = AppState::TETRIS_PLAYING;
        tetris_game_start(now_ms);
    }
}

void update_snake_playing(InputEvent event, uint32_t now_ms)
{
    if (event == InputEvent::BACK) {
        enter_menu();
        return;
    }
    if (event == InputEvent::PAUSE || event == InputEvent::SELECT) {
        app_state = AppState::SNAKE_PAUSED;
        snake_game_show_paused();
        return;
    }
    if (event == InputEvent::RESTART) {
        snake_game_start(now_ms);
        return;
    }

    snake_game_handle_direction(event);
    const SnakeTickResult result = snake_game_update(now_ms);
    if (result != SnakeTickResult::RUNNING) {
        app_state = AppState::SNAKE_GAME_OVER;
        snake_game_show_result(result == SnakeTickResult::WON);
    }
}

void update_snake_paused(InputEvent event, uint32_t now_ms)
{
    if (paused_menu_event(event)) {
        enter_menu();
    } else if (paused_restart_event(event)) {
        app_state = AppState::SNAKE_PLAYING;
        snake_game_start(now_ms);
    } else if (paused_resume_event(event)) {
        app_state = AppState::SNAKE_PLAYING;
        snake_game_resume(now_ms);
    }
}

void update_snake_game_over(InputEvent event, uint32_t now_ms)
{
    if (result_menu_event(event)) {
        enter_menu();
    } else if (result_restart_event(event)) {
        app_state = AppState::SNAKE_PLAYING;
        snake_game_start(now_ms);
    }
}

}  // namespace

void setup()
{
    display_hardware_init();
    Serial.begin(SERIAL_BAUD_RATE);
    serial_input_init();
    display_init();
    joystick_input_init();
    randomSeed(static_cast<uint32_t>(analogRead(A15)) ^ micros());

    print_startup_controls();
    enter_menu();
}

void loop()
{
    const uint32_t now_ms = millis();
    InputEvent event = serial_input_poll();
    if (event == InputEvent::NONE) {
        event = joystick_input_poll(now_ms);
    }

    switch (app_state) {
        case AppState::MENU:
            update_menu(event);
            break;
        case AppState::SNAKE_PLAYING:
            update_snake_playing(event, now_ms);
            break;
        case AppState::SNAKE_PAUSED:
            update_snake_paused(event, now_ms);
            break;
        case AppState::SNAKE_GAME_OVER:
            update_snake_game_over(event, now_ms);
            break;
        case AppState::TETRIS_PLAYING:
            update_tetris_playing(event, now_ms);
            break;
        case AppState::TETRIS_PAUSED:
            update_tetris_paused(event, now_ms);
            break;
        case AppState::TETRIS_GAME_OVER:
            update_tetris_game_over(event, now_ms);
            break;
        case AppState::BREAKOUT_PLAYING:
            update_breakout_playing(event, now_ms);
            break;
        case AppState::BREAKOUT_PAUSED:
            update_breakout_paused(event, now_ms);
            break;
        case AppState::BREAKOUT_GAME_OVER:
            update_breakout_game_over(event, now_ms);
            break;
        case AppState::RACING_PLAYING:
            update_racing_playing(event, now_ms);
            break;
        case AppState::RACING_PAUSED:
            update_racing_paused(event, now_ms);
            break;
        case AppState::RACING_GAME_OVER:
            update_racing_game_over(event, now_ms);
            break;
        case AppState::GAME_2048_PLAYING:
            update_2048_playing(event);
            break;
        case AppState::GAME_2048_PAUSED:
            update_2048_paused(event);
            break;
        case AppState::GAME_2048_OVER:
            update_2048_over(event);
            break;
        case AppState::SOKOBAN_LEVEL_SELECT:
            update_sokoban_level_select(event);
            break;
        case AppState::SOKOBAN_PLAYING:
            update_sokoban_playing(event);
            break;
        case AppState::SOKOBAN_PAUSED:
            update_sokoban_paused(event);
            break;
        case AppState::SOKOBAN_WON:
            update_sokoban_won(event);
            break;
        case AppState::LANDER_PLAYING:
            update_lander_playing(event, now_ms);
            break;
        case AppState::LANDER_PAUSED:
            update_lander_paused(event, now_ms);
            break;
        case AppState::LANDER_OVER:
            update_lander_over(event, now_ms);
            break;
    }
}
