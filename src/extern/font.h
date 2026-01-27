#pragma once

#include <stdint.h>


typedef struct {
    uint8_t width;
    uint8_t glyphs[];
} font_t;


//#define GLYPH_HEIGHT 10 // Height of each glyph in pixels
//extern const font_t font_5x10; // 5x10 pixel font

#define GLYPH_HEIGHT 16 // Height of each glyph in pixels
extern const font_t font_10x16;

