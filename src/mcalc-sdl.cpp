#include <SDL.h>
#include <algorithm>
#include <cstdio>

#include "drivers/font.h"
#include "drivers/lcd.h"
#include "drivers/text.h"

#include "libcalc/libcalc.h"

SDL_Window* gWindow = nullptr;

bool gWantsQuit = false;


//----------------------------------------------------------------------------------------
// "lcd" driver
//

#if 0
// mirroring picocalc-text-starter defines

// draws a character to the screen at the specified spot
// returns the width of the drawn character
uint8_t lcd_putc(int x, int y, uint8_t c)
{
    if (!gCharSurface)
        return 0;

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

    if (gCursorX >= WIDTH || gCurrColIx > kMaxColIx)
    {
        if (gCurrLineIx < kMaxLinesInBuf-1)
        {
            gLineEndX[gCurrLineIx] = gCursorX;
            ++gCurrLineIx;
        }

        gCursorX = 0;
        gCursorY += gFont->Height;

        // flip the sign of the column width so we know about the newline
        gColWidths[gCurrColIx] = -advance;
    }

    ++gCurrColIx;

    printf("inc_col: adv=%d; curs=%d,%d; col=%d (%d); line=%d; lineEnd=%d\n",
        advance, gCursorX, gCursorY,
        gCurrColIx, gColWidths[gCurrColIx],
        gCurrLineIx, gLineEndX[gCurrLineIx]);
}

void lcd_dec_column()
{
    if (gCurrColIx <= 0)
        return;

    lcd_erase_cursor();

    --gCurrColIx;

    int glyphWidth = gColWidths[gCurrColIx];
    const bool endline = (glyphWidth < 0);
    if (endline)
    {
        glyphWidth = -glyphWidth;

        --gCurrLineIx;
        gCursorX = gLineEndX[gCurrLineIx];

        gCursorY -= gFont->Height;
    }

    gCursorX -= glyphWidth;
    gCursorWidth = glyphWidth;

    printf("dec_col: width=%d; endl=%c, curs=%d,%d; col=%d (%d); line=%d; lineEnd=%d\n",
        glyphWidth, endline ? 'Y' : 'n', gCursorX, gCursorY,
        gCurrColIx, gColWidths[gCurrColIx],
        gCurrLineIx, gLineEndX[gCurrLineIx]);

    lcd_rect(gCursorX, gCursorY, glyphWidth, gFont->Height, gBgCol);

    lcd_draw_cursor();
}

void lcd_next_line()
{
    const int glyphHeight = gFont->Height;
    gCursorY += glyphHeight;

    while(gCursorY >= (HEIGHT - glyphHeight))
        lcd_scroll_up(glyphHeight);
}

void lcd_prev_line()
{
    

    const int glyphHeight = gFont->Height;
    gCursorY += glyphHeight;
}

void lcd_next_tab()
{
    const int tabwidth = 6 * gFont->Width;
    gCursorX += tabwidth + gFont->Width - 1;
    if (gCursorX >= (WIDTH - tabwidth))
    {
        gCursorX = 0;
        gCurrColIx = 0;
        lcd_next_line();
    }
    else
    {
        gCursorX -= (gCursorX % tabwidth);
    }
}

void lcd_emit(char c)
{
    lcd_erase_cursor();

    switch(c)
    {
    case SDLK_BACKSPACE:
        lcd_backspace();
        break;

    case '\t':
        lcd_next_tab();
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
            // TODO: refactor this; should all be in a single call!

            // if this char would end off the screen, advance to the next line immediately
            const GlyphMetric metric = font_get_glyph_metric(gFont, c, gMonospace);
            if (gCursorX + metric.Advance >= WIDTH)
            {
                lcd_inc_column(metric.Advance);
            }

            const uint8_t advance = lcd_putc(gCursorX, gCursorY, c);
            lcd_inc_column(advance);
        }
    }
}

void lcd_put_image(const uint16_t* pixels, uint32_t imgw, uint32_t imgh)
{
    lcd_erase_cursor();

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


#endif


void eval_input()
{
    char resBuf[1024];

    calc_eval(input_get_line(), resBuf, sizeof(resBuf));
    text_emit_str(resBuf);

    if (const Plot* plot = get_plot())
    {
        text_put_image(plot->Pixels, MC_PLOT_WIDTH, MC_PLOT_HEIGHT);
        reset_plot();
    }

    input_reset_line();
}

bool gToggleCursor = false;
Uint32 cursor_timer_func(Uint32 interval, void*)
{
    gToggleCursor = true;
    return interval;
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

static bool cmd_bye(const char*)
{
    gWantsQuit = true;
    return true;
}

static bool cmd_big(const char*)
{
    text_set_font(&font_10x16);
    return true;
}

static bool cmd_small(const char*)
{
    text_set_font(&font_5x10);
    return true;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

bool handle_input()
{
    SDL_Event evt;
    while (SDL_PollEvent(&evt))
    {
        switch (evt.type)
        {
        case SDL_QUIT:
            gWantsQuit = true;
            break;

        case SDL_TEXTINPUT:
            //printf("textinput: char=%c\n", evt.text.text[0]);
            input_process_char(evt.text.text[0]);
            break;

        case SDL_KEYDOWN:
            {
                const int keycode = evt.key.keysym.sym;
                const int scancode = evt.key.keysym.scancode;
                switch (keycode)
                {
                case SDLK_LEFT:
                case SDLK_RIGHT:
                case SDLK_BACKSPACE:
                case SDLK_DELETE:
                case SDLK_RETURN:
                    input_process_char(keycode);
                    if (input_has_complete_line())
                        eval_input();
                    break;
                }
                switch (scancode)
                {
                case SDL_SCANCODE_KP_ENTER:
                    input_process_char(SDLK_RETURN);
                    if (input_has_complete_line())
                        eval_input();
                    break;

                case SDL_SCANCODE_ESCAPE:
                    return false;
                }
            //    printf("keydown: keycode=%d, scancode=%d\n",
            //       evt.key.keysym.sym, evt.key.keysym.scancode);

            }
            break;

        default:
            break;
        }
    }

    return !gWantsQuit;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

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
    if (!lcd_init())
        return 1;

    calc_init(text_emit_str);
    register_calc_cmd(cmd_big, "big", "", "switches to big text");
    register_calc_cmd(cmd_small, "small", "", "switches to small text");
    register_calc_cmd(cmd_bye, "bye", "", "closes the calc");

    text_emit_str(MCALC_WELCOME);
    text_emit_str(">");

    lcd_refresh(gWindow);

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

    while (!gWantsQuit)
    {
        handle_input();

        if (gToggleCursor)
        {
            gToggleCursor = false;
            showCursor = !showCursor;
            if (showCursor)
                cursor_draw();
            else
                cursor_erase();
        }

        lcd_refresh(gWindow);
    }

    SDL_RemoveTimer(cursorTimer);

    lcd_cleanup();
    SDL_DestroyWindow(gWindow);
    SDL_Quit();

    return 0;
}



