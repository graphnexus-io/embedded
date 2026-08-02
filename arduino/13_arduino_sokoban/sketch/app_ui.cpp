#include "app_ui.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "display_hal.h"

namespace {

constexpr int16_t MODE_FIRST_Y = 104;
constexpr int16_t MODE_ROW_HEIGHT = 38;

const char *mode_label(uint8_t item)
{
    switch (item) {
        case 0U: return "CONTINUE";
        case 1U: return "FREE SELECT";
        case 2U: return "CAMPAIGN";
        default: return "";
    }
}

void format_level(uint16_t level_index, char *output)
{
    const uint16_t value = static_cast<uint16_t>(level_index + 1U);
    output[0] = static_cast<char>('0' + (value / 100U) % 10U);
    output[1] = static_cast<char>('0' + (value / 10U) % 10U);
    output[2] = static_cast<char>('0' + value % 10U);
    output[3] = '\0';
}

void draw_mode_entry(const SessionState &session, uint8_t item, bool selected)
{
    const int16_t y = static_cast<int16_t>(MODE_FIRST_Y + item * MODE_ROW_HEIGHT);
    display_fill_rect(118, y - 5, 244, 29, COLOR_LCD);

    char line[24];
    uint8_t position = 0U;
    line[position++] = selected ? '>' : ' ';
    line[position++] = ' ';
    const char *label = mode_label(item);
    while (*label != '\0' && position + 1U < sizeof(line)) {
        line[position++] = *label++;
    }
    line[position] = '\0';
    const uint16_t foreground =
        item == 0U && !session.active ? COLOR_INK_LIGHT : COLOR_INK;
    display_text(132, y, line, 2U, foreground, COLOR_LCD);
}

void draw_progress(const SessionState &session)
{
    char progress[34] = "CAMPAIGN ";
    uint8_t position = static_cast<uint8_t>(strlen(progress));
    if (session.campaign_next >= LEVEL_COUNT) {
        const char complete[] = "COMPLETE";
        memcpy(&progress[position], complete, sizeof(complete));
    } else {
        char level[4];
        format_level(session.campaign_next, level);
        memcpy(&progress[position], level, sizeof(level));
    }
    display_fill_rect(76, 225, 328, 18, COLOR_LCD);
    display_text(82, 227, progress, 1U, COLOR_INK_LIGHT, COLOR_LCD);

    char score[24] = "CAMPAIGN SCORE ";
    ultoa(session.campaign_score, &score[15], 10);
    display_fill_rect(76, 245, 328, 18, COLOR_LCD);
    display_text(82, 247, score, 1U, COLOR_INK_LIGHT, COLOR_LCD);
}

void draw_level_number(uint16_t level_index)
{
    char level[4];
    format_level(level_index, level);
    display_fill_rect(144, 100, 192, 74, COLOR_LCD_SHADE);
    display_draw_rect(144, 100, 192, 74, COLOR_INK);
    display_text(186, 118, level, 4U, COLOR_INK, COLOR_LCD_SHADE);

    const uint8_t crates = level_index < 5U ? 2U :
        level_index < 10U ? 3U :
        level_index < 40U ? 4U :
        level_index < 70U ? 5U : 6U;
    const char *rating = level_index < 5U ? "BEGINNER" :
        level_index < 10U ? "MEDIUM" : "SUPER EXPERT";
    char difficulty[24] = "CRATES 0  ";
    difficulty[7] = static_cast<char>('0' + crates);
    uint8_t position = static_cast<uint8_t>(strlen(difficulty));
    while (*rating != '\0' && position + 1U < sizeof(difficulty)) {
        difficulty[position++] = *rating++;
    }
    difficulty[position] = '\0';
    display_fill_rect(164, 182, 190, 16, COLOR_LCD);
    display_text(170, 184, difficulty, 1U, COLOR_INK, COLOR_LCD);
}

void draw_center_overlay(const char *title, const char *line_two, const char *line_three)
{
    display_draw_lcd_panel();
    display_fill_rect(82, 90, 316, 140, COLOR_LCD_SHADE);
    display_draw_rect(82, 90, 316, 140, COLOR_INK);
    const int16_t title_x = static_cast<int16_t>(
        240 - static_cast<int16_t>(strlen(title)) * 6);
    display_text(title_x, 112, title, 2U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(132, 162, line_two, 1U, COLOR_INK, COLOR_LCD_SHADE);
    display_text(132, 190, line_three, 1U, COLOR_INK, COLOR_LCD_SHADE);
}

}  // namespace

void ui_show_mode_menu(const SessionState &session, uint8_t selected_item)
{
    display_draw_lcd_panel();
    display_text(174, 42, "SOKOBAN 100", 2U, COLOR_INK, COLOR_LCD);
    display_text(174, 70, "CHOOSE MODE", 1U, COLOR_INK_LIGHT, COLOR_LCD);
    display_draw_rect(112, 92, 256, 128, COLOR_INK_LIGHT);
    for (uint8_t item = 0U; item < MODE_MENU_ITEM_COUNT; ++item) {
        draw_mode_entry(session, item, item == selected_item);
    }
    draw_progress(session);
    display_text(162, 278, "BUTTON/ENTER - SELECT", 1U, COLOR_INK, COLOR_LCD);
}

void ui_update_mode_menu_selection(
    const SessionState &session, uint8_t previous_item, uint8_t selected_item)
{
    draw_mode_entry(session, previous_item, false);
    draw_mode_entry(session, selected_item, true);
}

void ui_show_level_select(uint16_t level_index)
{
    display_draw_lcd_panel();
    display_text(138, 38, "FREE SELECT", 2U, COLOR_INK, COLOR_LCD);
    display_text(162, 70, "CHOOSE LEVEL", 1U, COLOR_INK_LIGHT, COLOR_LCD);
    draw_level_number(level_index);
    display_text(128, 216, "LEFT/RIGHT  -1/+1", 1U, COLOR_INK, COLOR_LCD);
    display_text(128, 236, "UP/DOWN    +10/-10", 1U, COLOR_INK, COLOR_LCD);
    display_text(158, 258, "5 BEGIN  5 MED  90 EXPERT", 1U,
                 COLOR_INK_LIGHT, COLOR_LCD);
    display_text(110, 280, "BUTTON START  HOLD BACK", 1U, COLOR_INK, COLOR_LCD);
}

void ui_update_level_select(uint16_t level_index)
{
    draw_level_number(level_index);
}

void ui_show_campaign_complete(uint32_t campaign_score)
{
    char score[26] = "FINAL SCORE ";
    ultoa(campaign_score, &score[12], 10);
    draw_center_overlay("100 COMPLETE", score, "BUTTON/HOLD - MODE MENU");
}

void ui_show_generation_error()
{
    draw_center_overlay("LEVEL ERROR", "GENERATION FAILED", "Q - MODE MENU");
}
