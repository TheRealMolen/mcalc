#pragma once

#include <stdint.h>

#define GLYPH_HEIGHT 10 // Height of each glyph in pixels

typedef struct {
    uint8_t width;
    uint8_t glyphs[];
} font_t;

extern const font_t font_5x10; // 5x10 pixel font
