#ifndef MINIOS_TEXT_EDITOR_H
#define MINIOS_TEXT_EDITOR_H

#include <Arduino.h>

#include "filesystem.h"

enum TextEditorExitResult : uint8_t {
    TEXT_EDITOR_EXIT_NONE = 0,
    TEXT_EDITOR_EXIT_CLOSED,
    TEXT_EDITOR_EXIT_DISCARDED,
};

FilesystemStatus text_editor_open(const char *normalized_path);
bool text_editor_active();
void text_editor_expect_crlf_tail();
void text_editor_poll();
TextEditorExitResult text_editor_take_exit_result();

#endif
