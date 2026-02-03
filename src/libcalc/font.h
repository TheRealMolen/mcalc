#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FONT_MAX_WIDTH  12
#define FONT_MAX_HEIGHT 16

typedef struct
{
    uint8_t Width;
    uint8_t Height;
    uint8_t Glyphs[];
} Font;


extern const Font font_5x10;
extern const Font font_10x16;

#ifdef __cplusplus
};
#endif

