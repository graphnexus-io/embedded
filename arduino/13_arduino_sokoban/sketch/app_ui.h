#ifndef SOKOBAN_100_APP_UI_H
#define SOKOBAN_100_APP_UI_H

#include <Arduino.h>

#include "session.h"

constexpr uint8_t MODE_MENU_ITEM_COUNT = 3U;

void ui_show_mode_menu(const SessionState &session, uint8_t selected_item);
void ui_update_mode_menu_selection(
    const SessionState &session, uint8_t previous_item, uint8_t selected_item);
void ui_show_level_select(uint16_t level_index);
void ui_update_level_select(uint16_t level_index);
void ui_show_campaign_complete(uint32_t campaign_score);
void ui_show_generation_error();

#endif
