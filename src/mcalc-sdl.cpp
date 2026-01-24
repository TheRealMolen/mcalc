#include <SDL.h>
#include <cstdio>

// picocalc-text-starter font
#include "extern/font.h"

SDL_Window* gWindow = nullptr;
SDL_Surface* gBackBuffer = nullptr;


//----------------------------------------------------------------------------------------
// "lcd" driver
//

// mirroring picocalc-text-starter defines
#define WIDTH   320
#define HEIGHT  320
#define ROWS    (HEIGHT / GLYPH_HEIGHT)

const font_t* gFont = &font_5x10;
constexpr uint8_t kFontWidth = 5;
#define COLS    (WIDTH / kFontWidth)

uint16_t gFgCol = 0xff07;
uint16_t gBgCol = 0x0000;

SDL_Surface* gCharSurface = nullptr;
bool init_lcd()
{
    gCharSurface = SDL_CreateRGBSurfaceWithFormat(0, kFontWidth, GLYPH_HEIGHT, 16, SDL_PIXELFORMAT_RGB565);
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

void lcd_putc(uint8_t column, uint8_t row, uint8_t c)
{
    if (!gCharSurface)
        return;

    if (SDL_MUSTLOCK(gCharSurface))
        SDL_LockSurface(gCharSurface);

    const uint8_t* glyph = &gFont->glyphs[c * GLYPH_HEIGHT];
    uint16_t* outPix = (uint16_t*)gCharSurface->pixels;
    const int pitch = gCharSurface->pitch / sizeof(*outPix);
    for (int i = 0; i < GLYPH_HEIGHT; ++i, ++glyph, outPix+=pitch)
    {
        const uint8_t g = *glyph;
        outPix[0] = (g & 0x10) ? gFgCol : gBgCol;
        outPix[1] = (g & 0x08) ? gFgCol : gBgCol;
        outPix[2] = (g & 0x04) ? gFgCol : gBgCol;
        outPix[3] = (g & 0x02) ? gFgCol : gBgCol;
        outPix[4] = (g & 0x01) ? gFgCol : gBgCol;
    }

    if (SDL_MUSTLOCK(gCharSurface))
        SDL_UnlockSurface(gCharSurface);
        
    SDL_Rect dstRect { column * kFontWidth, row * GLYPH_HEIGHT, 0, 0 };
    if (SDL_BlitSurface(gCharSurface, NULL, gBackBuffer, &dstRect) < 0)
    {
        fprintf(stderr, "BlitSurface error: %s\n", SDL_GetError());
    }
}





int main()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "Failed to init SDL: %s\n", SDL_GetError());
        return 1;
    }

    gWindow = SDL_CreateWindow("mcalc", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH*2, HEIGHT*2, SDL_WINDOW_SHOWN);
    if (!gWindow)
    {
        fprintf(stderr, "Failed to open window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    gBackBuffer = SDL_CreateRGBSurfaceWithFormat(0, WIDTH, HEIGHT, 16, SDL_PIXELFORMAT_RGB565);
    if (!gBackBuffer)
    {
        fprintf(stderr, "Failed to create RGB565 back buffer: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    if (!init_lcd())
        return 1;

    SDL_Surface* screenSurface = SDL_GetWindowSurface(gWindow);

    SDL_FillRect(gBackBuffer, NULL, 0);

    lcd_putc(0, 0, 'T');
    lcd_putc(1, 0, 'o');
    lcd_putc(2, 0, 'p');
    lcd_putc(3, 0, 'L');
    lcd_putc(0, ROWS-1, 'B');
    lcd_putc(1, ROWS-1, 't');
    lcd_putc(2, ROWS-1, 'm');
    lcd_putc(3, ROWS-1, 'L');
    lcd_putc(COLS-4, ROWS-1, 'B');
    lcd_putc(COLS-3, ROWS-1, 't');
    lcd_putc(COLS-2, ROWS-1, 'm');
    lcd_putc(COLS-1, ROWS-1, 'R');

    SDL_BlitScaled(gBackBuffer, NULL, screenSurface, NULL);
    SDL_UpdateWindowSurface(gWindow);

    bool wantsQuit = false;
    while (!wantsQuit)
    {
        SDL_Event evt;
        while (SDL_PollEvent(&evt))
        {
            if (evt.type == SDL_QUIT)
                wantsQuit = true;
        }
    }

    cleanup_lcd();
    SDL_FreeSurface(gBackBuffer);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();

    return 0;
}



