#include <Arduino.h>

#include "app_ui.h"
#include "config.h"
#include "display_hal.h"
#include "joystick_input.h"
#include "persistence.h"
#include "serial_input.h"
#include "session.h"
#include "sokoban_game.h"

enum class AppState : uint8_t {
    MODE_MENU,
    LEVEL_SELECT,
    PLAYING,
    PAUSED,
    LEVEL_COMPLETE,
    CAMPAIGN_COMPLETE,
    LEVEL_ERROR,
};

namespace {

SessionState session;
AppState app_state = AppState::MODE_MENU;
uint8_t selected_mode_item = 1U;
bool save_dirty = false;
uint32_t save_dirty_since_ms = 0UL;

void print_startup_help()
{
    Serial.print(F(APP_NAME));
    Serial.print(F(" "));
    Serial.println(F(APP_VERSION));
    Serial.println(F("WASD/arrows: move or navigate"));
    Serial.println(F("Enter: select, P: pause, R: restart"));
    Serial.println(F("U: undo, Q/Esc: back"));
    Serial.println(F("Backlight: steady full level (project 11 configuration)"));
    Serial.println(F("Joystick: move; short press select/pause; hold press back"));
}

void mark_save_dirty(uint32_t now_ms)
{
    save_dirty = true;
    save_dirty_since_ms = now_ms;
}

void save_now()
{
    if (persistence_save(session)) {
        save_dirty = false;
    } else {
        Serial.println(F("Warning: EEPROM save verification failed"));
    }
}

void enter_mode_menu()
{
    app_state = AppState::MODE_MENU;
    selected_mode_item = session.active ? 0U : 1U;
    ui_show_mode_menu(session, selected_mode_item);
}

void show_level_error()
{
    app_state = AppState::LEVEL_ERROR;
    ui_show_generation_error();
    Serial.println(F("Error: level generation or saved-state validation failed"));
}

bool start_level(uint16_t level_index, GameMode mode)
{
    if (mode == GameMode::FREE_SELECT) {
        session.free_level = level_index;
    }
    if (!sokoban_game_start(level_index, mode, &session)) {
        show_level_error();
        return false;
    }
    app_state = AppState::PLAYING;
    save_now();
    Serial.print(F("Level "));
    Serial.print(level_index + 1U);
    Serial.println(mode == GameMode::CAMPAIGN ? F(" campaign") : F(" free select"));
    return true;
}

void finish_level()
{
    session.active = false;
    if (session.mode == GameMode::CAMPAIGN &&
        session.level_index == session.campaign_next) {
        session.campaign_score += session.level_score;
        if (session.campaign_next < LEVEL_COUNT) {
            ++session.campaign_next;
        }
    }
    if (session.mode == GameMode::FREE_SELECT) {
        session.free_level = session.level_index;
    }
    save_now();
    app_state = AppState::LEVEL_COMPLETE;
    sokoban_game_show_complete(session);
    Serial.print(F("Level clear. Score: "));
    Serial.println(session.level_score);
}

void resume_saved_game()
{
    if (!session.active) {
        return;
    }
    if (!sokoban_game_restore(&session)) {
        session.active = false;
        save_now();
        show_level_error();
        return;
    }
    if (sokoban_game_is_solved()) {
        finish_level();
    } else {
        app_state = AppState::PLAYING;
    }
}

void update_mode_menu(InputEvent event)
{
    const uint8_t previous = selected_mode_item;
    if (event == InputEvent::UP) {
        selected_mode_item = selected_mode_item == 0U ?
            static_cast<uint8_t>(MODE_MENU_ITEM_COUNT - 1U) :
            static_cast<uint8_t>(selected_mode_item - 1U);
    } else if (event == InputEvent::DOWN) {
        selected_mode_item = static_cast<uint8_t>(
            (selected_mode_item + 1U) % MODE_MENU_ITEM_COUNT);
    } else if (event == InputEvent::SELECT) {
        if (selected_mode_item == 0U) {
            resume_saved_game();
        } else if (selected_mode_item == 1U) {
            app_state = AppState::LEVEL_SELECT;
            ui_show_level_select(session.free_level);
        } else {
            if (session.campaign_next >= LEVEL_COUNT) {
                app_state = AppState::CAMPAIGN_COMPLETE;
                ui_show_campaign_complete(session.campaign_score);
            } else {
                start_level(session.campaign_next, GameMode::CAMPAIGN);
            }
        }
        return;
    }
    if (selected_mode_item != previous) {
        ui_update_mode_menu_selection(session, previous, selected_mode_item);
    }
}

void adjust_free_level(int16_t delta, uint32_t now_ms)
{
    int16_t candidate = static_cast<int16_t>(session.free_level) + delta;
    if (candidate < 0) {
        candidate = 0;
    } else if (candidate >= static_cast<int16_t>(LEVEL_COUNT)) {
        candidate = static_cast<int16_t>(LEVEL_COUNT - 1U);
    }
    if (candidate != static_cast<int16_t>(session.free_level)) {
        session.free_level = static_cast<uint16_t>(candidate);
        ui_update_level_select(session.free_level);
        mark_save_dirty(now_ms);
    }
}

void update_level_select(InputEvent event, uint32_t now_ms)
{
    switch (event) {
        case InputEvent::LEFT: adjust_free_level(-1, now_ms); break;
        case InputEvent::RIGHT: adjust_free_level(1, now_ms); break;
        case InputEvent::UP: adjust_free_level(10, now_ms); break;
        case InputEvent::DOWN: adjust_free_level(-10, now_ms); break;
        case InputEvent::PAGE_PREVIOUS: adjust_free_level(-100, now_ms); break;
        case InputEvent::PAGE_NEXT: adjust_free_level(100, now_ms); break;
        case InputEvent::SELECT:
            start_level(session.free_level, GameMode::FREE_SELECT);
            break;
        case InputEvent::BACK:
            if (save_dirty) {
                save_now();
            }
            enter_mode_menu();
            break;
        default:
            break;
    }
}

void update_playing(InputEvent event, uint32_t now_ms)
{
    if (event == InputEvent::BACK) {
        save_now();
        enter_mode_menu();
        return;
    }
    if (event == InputEvent::PAUSE || event == InputEvent::SELECT) {
        save_now();
        app_state = AppState::PAUSED;
        sokoban_game_show_paused();
        return;
    }
    if (event == InputEvent::RESTART) {
        start_level(session.level_index, session.mode);
        return;
    }

    const SokobanActionResult result =
        sokoban_game_handle_input(event, &session);
    if (result == SokobanActionResult::CHANGED) {
        mark_save_dirty(now_ms);
    } else if (result == SokobanActionResult::WON) {
        finish_level();
    }
}

void update_paused(InputEvent event)
{
    if (event == InputEvent::BACK) {
        enter_mode_menu();
    } else if (event == InputEvent::RESTART) {
        start_level(session.level_index, session.mode);
    } else if (event == InputEvent::PAUSE || event == InputEvent::SELECT) {
        app_state = AppState::PLAYING;
        sokoban_game_redraw(session);
    }
}

void update_level_complete(InputEvent event)
{
    if (event == InputEvent::BACK) {
        enter_mode_menu();
    } else if (event == InputEvent::RESTART) {
        start_level(session.level_index, session.mode);
    } else if (event == InputEvent::SELECT) {
        if (session.mode == GameMode::CAMPAIGN) {
            if (session.campaign_next >= LEVEL_COUNT) {
                app_state = AppState::CAMPAIGN_COMPLETE;
                ui_show_campaign_complete(session.campaign_score);
            } else {
                start_level(session.campaign_next, GameMode::CAMPAIGN);
            }
        } else {
            if (session.free_level + 1U < LEVEL_COUNT) {
                ++session.free_level;
            }
            app_state = AppState::LEVEL_SELECT;
            ui_show_level_select(session.free_level);
            mark_save_dirty(millis());
        }
    }
}

void update_terminal_screen(InputEvent event)
{
    if (event == InputEvent::BACK || event == InputEvent::SELECT) {
        enter_mode_menu();
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
    const bool save_found = persistence_load(&session);
    print_startup_help();
    Serial.println(save_found ? F("EEPROM save loaded") : F("No EEPROM save found"));
    enter_mode_menu();
}

void loop()
{
    const uint32_t now_ms = millis();
    InputEvent event = serial_input_poll();
    if (event == InputEvent::NONE) {
        event = joystick_input_poll(now_ms);
    }

    switch (app_state) {
        case AppState::MODE_MENU:
            update_mode_menu(event);
            break;
        case AppState::LEVEL_SELECT:
            update_level_select(event, now_ms);
            break;
        case AppState::PLAYING:
            update_playing(event, now_ms);
            break;
        case AppState::PAUSED:
            update_paused(event);
            break;
        case AppState::LEVEL_COMPLETE:
            update_level_complete(event);
            break;
        case AppState::CAMPAIGN_COMPLETE:
        case AppState::LEVEL_ERROR:
            update_terminal_screen(event);
            break;
    }

    if (save_dirty &&
        static_cast<uint32_t>(now_ms - save_dirty_since_ms) >= SAVE_IDLE_DELAY_MS) {
        save_now();
    }
}
