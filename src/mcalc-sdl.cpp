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

bool gMonospace = false;

int gCursorX = 0;
int gCursorY = 0;

constexpr int gMaxColIx = (WIDTH/2) - 1;
int gCurrColIx = 0;
uint8_t gColWidths[gMaxColIx+1];

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


void lcd_erase_cursor()
{
    SDL_Rect rect { gCursorX, gCursorY+gFont->Height-1, gFont->Width-1, 1 };
    SDL_FillRect(gBackBuffer, &rect, gBgCol);
}

void lcd_draw_cursor()
{
    SDL_Rect rect { gCursorX, gCursorY+gFont->Height-1, gFont->Width-1, 1 };
    SDL_FillRect(gBackBuffer, &rect, gFgCol);
}

// draws a character to the screen at the specified spot
// returns the width of the drawn character
uint8_t lcd_putc(int x, int y, uint8_t c)
{
    if (!gCharSurface)
        return x;

    if (SDL_MUSTLOCK(gCharSurface))
        SDL_LockSurface(gCharSurface);

    font_rasterise_char(gFont, c, gFgCol, gBgCol,
        (uint16_t*)gCharSurface->pixels,
        gCharSurface->pitch / sizeof(uint16_t), gCharSurface->h,
        0, 0);
        
    if (SDL_MUSTLOCK(gCharSurface))
        SDL_UnlockSurface(gCharSurface);
        
    const GlyphMetric metric = font_get_glyph_metric(gFont, c, gMonospace);
    SDL_Rect srcRect { metric.Skip, 0, metric.Advance, gFont->Height };
    SDL_Rect dstRect { x, y, 0, 0 };
    if (SDL_BlitSurface(gCharSurface, &srcRect, gBackBuffer, &dstRect) < 0)
    {
        fprintf(stderr, "BlitSurface error: %s\n", SDL_GetError());
    }

    return metric.Advance;
}

void lcd_inc_column(uint8_t advance)
{
    gCursorX += advance;

    gColWidths[gCurrColIx] = advance;
    ++gCurrColIx;

    if (gCursorX >= WIDTH || gCurrColIx > gMaxColIx)
    {
        gCursorX = 0;
        gCursorY += gFont->Height;

        // TODO: this breaks backspace from one line to the previous
        // ideally we'd remember the start ix of each line and only reset when flushing
        gCurrColIx = 0;
    }
}

void lcd_backspace()
{
    if (gCurrColIx <= 0)
        return;

    lcd_erase_cursor();

    --gCurrColIx;

    const int glyphWidth = gColWidths[gCurrColIx];
    gCursorX -= glyphWidth;

    SDL_Rect rect { gCursorX, gCursorY, glyphWidth, gFont->Height };
    SDL_FillRect(gBackBuffer, &rect, gBgCol);

    lcd_draw_cursor();
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

    lcd_draw_cursor();
}

void lcd_next_line()
{
    const int glyphHeight = gFont->Height;
    gCursorY += glyphHeight;

    while(gCursorY >= (HEIGHT - glyphHeight))
        lcd_scroll_up(glyphHeight);
}

void lcd_emit(char c)
{
    lcd_erase_cursor();

    switch(c)
    {
    case SDLK_BACKSPACE:
        lcd_backspace();
        break;

    case SDLK_RETURN:
    case '\n':
        gCursorX = 0;
        gCurrColIx = 0;
        lcd_next_line();
        break;

    default:
        if (c >= 0x20 && c < 0x7f)
        {
            const uint8_t advance = lcd_putc(gCursorX, gCursorY, c);
            lcd_inc_column(advance);
        }
    }

    lcd_draw_cursor();
}

void lcd_put_image(const uint16_t* pixels, uint32_t imgw, uint32_t imgh)
{
    // scroll up enough so there's at least imgh pixels free to draw on
    // nb. we're over-clearing the back buf at this point as we're about to blat over a chunk with the img
    int line_btm = gCursorY;
    int img_top = HEIGHT - imgh;
    if (line_btm > img_top)
    {
        lcd_scroll_up(line_btm - img_top);
        line_btm = gCursorY;
    }
    img_top = std::min(line_btm, img_top);

    uint16_t* pixels_nonconst = const_cast<uint16_t*>(pixels);
    SDL_Surface* img_surf = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels_nonconst, imgw, imgh, 16, imgw * sizeof(pixels[0]), SDL_PIXELFORMAT_RGB565);
    if (!img_surf)
        return;

    const int img_left = int(WIDTH - imgw - 1);
    SDL_Rect dstRect { img_left, img_top, int(imgw), int(imgh) };
    SDL_BlitSurface(img_surf, nullptr, gBackBuffer, &dstRect);

    SDL_FreeSurface(img_surf);

    gCursorY += imgh;
}


void display_puts(const char* s)
{
    if (!s)
        return;

    while (*s)
    {
        lcd_emit(*s);
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

        lcd_emit(SDLK_RETURN);

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

        lcd_emit(c);
    }

    return false;
}


void eval_input()
{
    char resBuf[1024];
    calc_eval(gReadBuf, resBuf, sizeof(resBuf));
    display_puts(resBuf);

    if (const Plot* plot = get_plot())
    {
        lcd_put_image(plot->Pixels, MC_PLOT_WIDTH, MC_PLOT_HEIGHT);
        reset_plot();
    }

    display_puts("\n>");

    gReadBufIx = 0;
    gReadBuf[0] = 0;
}

bool gToggleCursor = false;
Uint32 cursor_timer_func(Uint32 interval, void*)
{
    gToggleCursor = true;
    return interval;
}


int main()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
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

    display_puts("molencalc v15     don't panic\n\n");
    display_puts(">");

    SDL_BlitScaled(gBackBuffer, NULL, screenSurface, NULL);
    SDL_UpdateWindowSurface(gWindow);

    SDL_TimerID cursorTimer = SDL_AddTimer(500, cursor_timer_func, nullptr);
    bool showCursor = true;

#if 0
    const char* initText = "f: x -> sin(x)/x\n:g f -10<x<10, -0.5<y<1.5";
    for (const char* c = initText; *c; ++c)
    {
        if (handleInputChar(*c))
            eval_input();
    }
#endif

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

        if (gToggleCursor)
        {
            gToggleCursor = false;
            showCursor = !showCursor;
            if (showCursor)
                lcd_draw_cursor();
            else
                lcd_erase_cursor();
        }

        SDL_BlitScaled(gBackBuffer, NULL, screenSurface, NULL);
        SDL_UpdateWindowSurface(gWindow);

    }

    SDL_RemoveTimer(cursorTimer);

    cleanup_lcd();
    SDL_FreeSurface(gBackBuffer);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();

    return 0;
}



