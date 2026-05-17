//
// SDL reimplementation of roughly-equivalent LCD driver for Picocalc
//

// has a 320x480 scrollable back buffer and "hardware" scrolling to try and mimic the real hw

#include <SDL.h>

#include "font.h"
#include "lcd.h"

//----------------------------------------------------------------------------------------

#define FRAME_HEIGHT    (480)

//----------------------------------------------------------------------------------------

SDL_Surface* gFrameSurface = nullptr;   // the equivalent of the 320x480 scrollable frame in the lcd
SDL_Surface* gBackBuffer = nullptr;     // the equivalent of the screen pixels on the lcd

static uint16_t lcd_y_offset = 0;  // offset for vertical scrolling

//----------------------------------------------------------------------------------------

bool lcd_init()
{
    gFrameSurface = SDL_CreateRGBSurfaceWithFormat(0, WIDTH, FRAME_HEIGHT, 16, SDL_PIXELFORMAT_RGB565);
    if (!gFrameSurface)
    {
        fprintf(stderr, "Failed to create RGB565 frame surface: %s\n", SDL_GetError());
        return false;
    }
    gBackBuffer = SDL_CreateRGBSurfaceWithFormat(0, WIDTH, HEIGHT, 16, SDL_PIXELFORMAT_RGB565);
    if (!gBackBuffer)
    {
        fprintf(stderr, "Failed to create RGB565 back buffer: %s\n", SDL_GetError());
        return false;
    }

    SDL_FillRect(gFrameSurface, NULL, 0);
    SDL_FillRect(gBackBuffer, NULL, 0);

    return true;
}

void lcd_cleanup()
{
    SDL_FreeSurface(gFrameSurface);
    SDL_FreeSurface(gBackBuffer);
}

//-------------------------------------------------------------------------------------------------

#ifdef MLN_TARGET_PC
void lcd_refresh(SDL_Window* window)
{
    SDL_Rect src { 0, 0, WIDTH, HEIGHT };
    SDL_Rect dest { 0, 0, WIDTH, HEIGHT };

    static int lastLoggedOffs = -1;
    const bool shouldLog = lcd_y_offset != lastLoggedOffs;
    lastLoggedOffs = lcd_y_offset;

    if (shouldLog)
        printf("\nrefresh, y_offs=%d\n", lcd_y_offset);

    // step 1: render the top part of the screen, scrolled up by the current scroll offset
    src.y = lcd_y_offset;
    src.h = FRAME_HEIGHT - lcd_y_offset;
    dest.y = 0; // -lcd_y_offset * screenScale;
    dest.h = src.h;
    if (shouldLog)
        printf("blit1: src(%d, %d, %d, %d)  -> dst(%d, %d, %d, %d)\n",
            src.x, src.y, src.w, src.h, dest.x, dest.y, dest.w, dest.h);
    SDL_BlitSurface(gFrameSurface, &src, gBackBuffer, &dest);

    // step 2: render the bottom part of the screen
    src.h = HEIGHT - src.y;
    src.y = 0;
    dest.y = (FRAME_HEIGHT - lcd_y_offset);
    dest.h = src.h;
    if (shouldLog)
        printf("blit2: src(%d, %d, %d, %d)  -> dst(%d, %d, %d, %d)\n",
            src.x, src.y, src.w, src.h, dest.x, dest.y, dest.w, dest.h);
    if (src.h > 0)
        SDL_BlitSurface(gFrameSurface, &src, gBackBuffer, &dest);

    SDL_Surface* screenSurface = SDL_GetWindowSurface(window);
    SDL_BlitScaled(gBackBuffer, nullptr, screenSurface, nullptr);
    SDL_UpdateWindowSurface(window);
}
#endif

//----------------------------------------------------------------------------------------

void lcd_rect(int x, int y, int w, int h, uint16_t col)
{
    //TODO: cope with rect wrapping around Y
    SDL_Rect rect { x, y + lcd_y_offset, w, h };
    SDL_FillRect(gFrameSurface, &rect, col);
}



void lcd_define_scrolling([[maybe_unused]] uint16_t top_fixed_area, [[maybe_unused]] uint16_t bottom_fixed_area)
{
    throw "unimplemented";
}

void lcd_scroll_reset()
{
    lcd_y_offset = 0;
}

void lcd_scroll_clear(uint16_t col)
{
    lcd_scroll_reset();

    lcd_rect(0, 0, WIDTH, HEIGHT, col);
}


// Scroll the screen up (make space at the bottom)
void lcd_scroll_up(uint32_t distance, uint16_t clearCol)
{
    // This will rotate the content in the scroll area up by one line
    lcd_y_offset = (lcd_y_offset + distance) % FRAME_HEIGHT;

    // Clear the new line at the bottom
    // FIXME: this doesn't draw in the right place - need to think about the actual location to clear
    lcd_rect(0, HEIGHT - distance, WIDTH, distance, clearCol);
}

// Scroll the screen down one line (making space at the top)
void lcd_scroll_down(uint32_t distance, uint16_t clearCol)
{
    // This will rotate the content in the scroll area down by one line
    lcd_y_offset = (lcd_y_offset - distance + FRAME_HEIGHT) % FRAME_HEIGHT;

    // Clear the new line at the top
    lcd_rect(0, 0, WIDTH, distance, clearCol);
}



void lcd_blit(const uint16_t *pixels, int x, int y, int width, int height)
{
    constexpr int depth = sizeof(*pixels) * 8;
    const int pitch = width * sizeof(*pixels);

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uint16_t*>(pixels), width, height, depth, pitch, SDL_PIXELFORMAT_RGB565);

    if (y >= 0 && y < HEIGHT)
    {
        // Adjust y for vertical scroll offset and wrap within memory height
        int y_virtual = (lcd_y_offset + y) % FRAME_HEIGHT;
        int y_end = y_virtual + height - 1;

//        fixme - need to split the blit top & bottom
        if (y_end > FRAME_HEIGHT)
        {
            y_end = FRAME_HEIGHT - 1;
        }

        SDL_Rect dest { x, y_virtual, 0, 0 };
        SDL_BlitSurface(surf, nullptr, gFrameSurface, &dest);

//        lcd_set_window(x, lcd_scroll_top + y_virtual, x + width - 1, y_end);
    }
    else
    {
        // No vertical scrolling
        SDL_Rect dest { x, y, width, height };
        SDL_BlitSurface(surf, nullptr, gFrameSurface, &dest);
    }

    SDL_FreeSurface(surf);
}


