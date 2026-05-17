//
// SDL reimplementation of roughly-equivalent LCD driver for Picocalc
//

// has a 320x480 scrollable back buffer and "hardware" scrolling to try and mimic the real hw

#include <SDL.h>

#include "font.h"
#include "lcd.h"

//----------------------------------------------------------------------------------------

SDL_Surface* gBackBuffer = nullptr;

const Font* gFont = &font_10x16;
//const Font* gFont = &font_5x10;

bool gMonospace = false;

int16_t gCursorX = 0;
int16_t gCursorY = 0;
int8_t gCursorWidth = gFont->Width;

constexpr int kMaxColIx = (WIDTH/2) - 1;
constexpr int kMaxLinesInBuf = 16;
int8_t gColWidths[kMaxColIx+1];
uint16_t gLineEndX[kMaxLinesInBuf];
int16_t gCurrColIx = 0;
int16_t gCurrLineIx = 0;


uint16_t gFgCol = 0xff0a;
uint16_t gBgCol = 0x0000;

SDL_Surface* gCharSurface = nullptr;
bool init_lcd()
{
    gCharSurface = SDL_CreateRGBSurfaceWithFormat(0, FONT_MAX_WIDTH, FONT_MAX_HEIGHT, 16, SDL_PIXELFORMAT_RGB565);
    if (!gCharSurface)
    {
        fprintf(stderr, "Failed to create tiny char surface: %s\n", SDL_GetError());
        return false;
    }
    return true;
}
void cleanup_lcd()
{
    if (gCharSurface)
    {
        SDL_FreeSurface(gCharSurface);
        gCharSurface = nullptr;
    }
}


void lcd_set_font(const Font* font)
{
    gFont = font;
}

void lcd_rect(int x, int y, int w, int h, uint16_t col)
{
    SDL_Rect rect { x, y, w, h };
    SDL_FillRect(gBackBuffer, &rect, col);
}


void lcd_erase_cursor()
{
    lcd_rect(gCursorX, gCursorY+gFont->Height-1, gCursorWidth-1, 1, gBgCol);
}

void lcd_draw_cursor()
{
    lcd_rect(gCursorX, gCursorY+gFont->Height-1, gCursorWidth-1, 1, gFgCol);
}

void lcd_scroll_up(uint32_t distance)
{
    lcd_erase_cursor();

    SDL_Rect srcRect { 0, int(distance), WIDTH, HEIGHT };
    SDL_Rect dstRect { 0, 0, WIDTH, HEIGHT };
    SDL_BlitSurface(gBackBuffer, &srcRect, gBackBuffer, &dstRect);

    SDL_Rect clrRect { 0, HEIGHT-int(distance), WIDTH, int(distance) };
    SDL_FillRect(gBackBuffer, &clrRect, gBgCol);

    if (gCursorY > int(distance))
        gCursorY -= distance;
    else
        gCursorY = 0;
}



