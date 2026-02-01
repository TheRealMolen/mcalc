#include <SDL.h>
#include <algorithm>
#include <cstdio>

#include "libcalc/libcalc.h"
#include "libcalc/font.h"

SDL_Window* gWindow = nullptr;
SDL_Surface* gBackBuffer = nullptr;


//----------------------------------------------------------------------------------------
// "lcd" driver
//

// mirroring picocalc-text-starter defines
#define WIDTH   320
#define HEIGHT  320

const Font* gFont = &font_10x16;
//const Font* gFont = &font_5x10;


uint16_t gFgCol = 0xff07;
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


struct GlyphMetric
{
    int Skip = 0;
    int Advance = 0;
};

GlyphMetric font_rasterise_char(const Font* font, char c, uint16_t* buf, int bufw, int bufh, int x=0, int y=0)
{
    const int glyphWidth = font->Width;
    const int fullGlyphHeight = font->Height;
    const int bytesPerGlyphRow = (glyphWidth + 7) / 8;

    const uint8_t* glyph = &font->Glyphs[c * fullGlyphHeight * bytesPerGlyphRow];

    uint16_t* outPix = buf + x + (y*bufw);
    const int pitch = bufw / sizeof(*buf);
    
    const int glyphHeight = std::min(fullGlyphHeight, bufh-y);

    for (int i = 0; i < glyphHeight; ++i, glyph+=bytesPerGlyphRow, outPix+=pitch)
    {
        uint16_t g = *glyph;

        if (glyphWidth == 5)
        {
            outPix[0] = (g & 0x10) ? gFgCol : gBgCol;
            outPix[1] = (g & 0x08) ? gFgCol : gBgCol;
            outPix[2] = (g & 0x04) ? gFgCol : gBgCol;
            outPix[3] = (g & 0x02) ? gFgCol : gBgCol;
            outPix[4] = (g & 0x01) ? gFgCol : gBgCol;
        }
        else if (glyphWidth == 10)
        {
            g = (g << 8) | glyph[1];

            outPix[0] = (g & 0x200) ? gFgCol : gBgCol;
            outPix[1] = (g & 0x100) ? gFgCol : gBgCol;
            outPix[2] = (g & 0x080) ? gFgCol : gBgCol;
            outPix[3] = (g & 0x040) ? gFgCol : gBgCol;
            outPix[4] = (g & 0x020) ? gFgCol : gBgCol;
            outPix[5] = (g & 0x010) ? gFgCol : gBgCol;
            outPix[6] = (g & 0x008) ? gFgCol : gBgCol;
            outPix[7] = (g & 0x004) ? gFgCol : gBgCol;
            outPix[8] = (g & 0x002) ? gFgCol : gBgCol;
            outPix[9] = (g & 0x001) ? gFgCol : gBgCol;
        }
    }

    if (font == &font_10x16)
    {
        if (c == '\'' || c == '.' || c == ',' || c == ';' || c == ':')
            return { .Skip = 3, .Advance = glyphWidth - 7 };
        if (c == 'i' || c == 'l' || c == '1')
            return { .Skip = 2, .Advance = glyphWidth - 4 };
        if (c == 'f' || c == 't')
            return { .Skip = 1, .Advance = glyphWidth - 2 };
    }
    else if (font == &font_5x10)
    {
        if (c == 'm' || c == '/' || c == 'w' || c == 'v' || c == 'W' || c == 'V')
            return { .Advance = glyphWidth + 1 };
        if (c == 'l' || c == 'I' || c == '1')
            return { .Skip = 1, .Advance = glyphWidth - 1 };
    }

    return { .Advance = glyphWidth };
}


// draws a character to the screen at the specified spot
// returns the width of the drawn character
int lcd_putc(int x, int y, uint8_t c)
{
    if (!gCharSurface)
        return x;

    if (SDL_MUSTLOCK(gCharSurface))
        SDL_LockSurface(gCharSurface);

    const GlyphMetric metric = font_rasterise_char(gFont, c,
        (uint16_t*)gCharSurface->pixels,
        gCharSurface->pitch, gCharSurface->h,
        0, 0);
        
    if (SDL_MUSTLOCK(gCharSurface))
        SDL_UnlockSurface(gCharSurface);
        
    SDL_Rect srcRect { metric.Skip, 0, metric.Advance, gFont->Height };
    SDL_Rect dstRect { x, y, 0, 0 };
    if (SDL_BlitSurface(gCharSurface, &srcRect, gBackBuffer, &dstRect) < 0)
    {
        fprintf(stderr, "BlitSurface error: %s\n", SDL_GetError());
    }

    return metric.Advance;
}

int gCursorX = 0;
int gCursorY = 0;

void lcd_inc_column(int advance)
{
    gCursorX += advance;

    if (gCursorX >= WIDTH)
    {
        gCursorX = 0;
        gCursorY += gFont->Height;
    }
}

void lcd_erase_cursor()
{
    // TODO
}

void lcd_show_cursor()
{
    // TODO
}

void lcd_scroll_up_one_line()
{
    const int glyphHeight = gFont->Height;

    SDL_Rect srcRect { 0, glyphHeight, WIDTH, HEIGHT };
    SDL_Rect dstRect { 0, 0, WIDTH, HEIGHT };
    SDL_BlitSurface(gBackBuffer, &srcRect, gBackBuffer, &dstRect);

    SDL_Rect clrRect { 0, HEIGHT-glyphHeight, WIDTH, glyphHeight };
    SDL_FillRect(gBackBuffer, &clrRect, gBgCol);

    if (gCursorY > glyphHeight)
        gCursorY -= glyphHeight;
    else
        gCursorY = 0;
}

void lcd_next_line()
{
    const int glyphHeight = gFont->Height;
    gCursorY += glyphHeight;

    while(gCursorY >= (HEIGHT - glyphHeight))
        lcd_scroll_up_one_line();
}

void display_emit(char c)
{
    lcd_erase_cursor();

    switch(c)
    {
    case SDLK_BACKSPACE:
        if (gCursorX > 0)
            gCursorX -= gFont->Width;
        break;

    case SDLK_RETURN:
    case '\n':
        gCursorX = 0;
        lcd_next_line();
        break;

    default:
        if (c >= 0x20 && c < 0x7f)
        {
            const int advance = lcd_putc(gCursorX, gCursorY, c);
            lcd_inc_column(advance);
        }
    }
}

void display_puts(const char* s)
{
    if (!s)
        return;

    while (*s)
    {
        display_emit(*s);
        ++s;
    }
}


char gReadBuf[256] = {0};
constexpr int kReadBufSize = sizeof(gReadBuf) / sizeof(gReadBuf[0]);
int gReadBufIx = 0;

// returns true if the input ended a command that should be processed
bool handleInputChar(char c)
{
    if (c == '\r' || c == '\n')
    {
        if (gReadBufIx == 0)
            return false;

        display_emit(SDLK_RETURN);

        // null-terminate and return executable
        gReadBuf[gReadBufIx] = 0;
        return true;
    }

    if (c == SDLK_BACKSPACE)
    {
        if (gReadBufIx > 0)
        {
            --gReadBufIx;
            gReadBuf[gReadBufIx] = 0; // blank out whatever was there

            // erase the last char on screen
            display_puts("\b \b");
        }
    }
    else if ((gReadBufIx+1 < kReadBufSize) && (c >= 0x20) && (c < 0x7f))
    {
        gReadBuf[gReadBufIx] = c;
        ++gReadBufIx;

        display_emit(c);
    }

    return false;
}


void eval_input()
{
    char resBuf[1024];
    calc_eval(gReadBuf, resBuf, sizeof(resBuf));
    display_puts(resBuf);
    display_puts("\n>");

    gReadBufIx = 0;
    gReadBuf[0] = 0;
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

    display_puts("molencalc v12     don't panic\n\n");
    display_puts(">");

    SDL_BlitScaled(gBackBuffer, NULL, screenSurface, NULL);
    SDL_UpdateWindowSurface(gWindow);

    bool wantsQuit = false;
    while (!wantsQuit)
    {
        SDL_Event evt;
        while (SDL_PollEvent(&evt))
        {
            switch (evt.type)
            {
            case SDL_QUIT:
                wantsQuit = true;
                break;

            case SDL_TEXTINPUT:
                //printf("textinput: char=%c\n", evt.text.text[0]);
                handleInputChar(evt.text.text[0]);
                break;

            case SDL_KEYDOWN:
                {
                    const int keycode = evt.key.keysym.sym;
                    const int scancode = evt.key.keysym.scancode;
                    switch (keycode)
                    {
                    case SDLK_BACKSPACE:
                    case SDLK_DELETE:
                    case SDLK_RETURN:
                        if (handleInputChar(keycode) && (gReadBufIx > 0))
                            eval_input();
                        break;
                    }
                    switch (scancode)
                    {
                    case SDL_SCANCODE_KP_ENTER:
                        if (handleInputChar(SDLK_RETURN) && (gReadBufIx > 0))
                            eval_input();
                        break;
                    }
                //    printf("keydown: keycode=%d, scancode=%d\n",
                //       evt.key.keysym.sym, evt.key.keysym.scancode);

                }
                break;

            default:
                break;
            }
        }

        SDL_BlitScaled(gBackBuffer, NULL, screenSurface, NULL);
        SDL_UpdateWindowSurface(gWindow);

    }

    cleanup_lcd();
    SDL_FreeSurface(gBackBuffer);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();

    return 0;
}



