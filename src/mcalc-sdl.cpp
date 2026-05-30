#include "platform.h"

#include <SDL.h>
#include <algorithm>
#include <cstdio>

#include "drivers/font.h"
#include "drivers/gfx.h"
#include "drivers/keyboard.h"
#include "drivers/history.h"
#include "drivers/lcd.h"
#include "drivers/palette.h"
#include "drivers/text.h"

#include "libcalc/libcalc.h"
#include "libcalc/palette.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include "extern/doctest.h"

//-------------------------------------------------------------------------------------------------

SDL_Window* gWindow = nullptr;

bool gWantsQuit = false;

//-------------------------------------------------------------------------------------------------

void eval_input()
{
    char resBuf[1024];

    reset_plot();

    calc_eval(input_get_line(), resBuf, sizeof(resBuf));

    text_emit_str(resBuf);

    if (const Plot* plot = get_plot())
    {
        text_put_image(plot->Pixels, MC_PLOT_WIDTH, MC_PLOT_HEIGHT);
    }

    text_emit_str("\n");
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

static bool cmd_screenshot(const char*)
{
    col8_t pixels[WIDTH*HEIGHT];

    //lcd_readback(0, 0, WIDTH, HEIGHT, pixels);
    fb_readback(0, 0, WIDTH, HEIGHT, pixels);

    const Palette* pal = gfx_get_palette();
    if (!pal)
    {
        text_emit_str("\nerr: no palette\n");
        return false;
    }
    constexpr int numCols = 256;
    uint8_t palRgb[numCols * 4];
    pal->ExportAsBGRQuads(palRgb, numCols);

   /* {
        char head[1024];
        sprintf(head, "data starts: %04x %04x %04x %04x\n"
                      "             %04x %04x %04x %04x\n",
            int(pixels[0]), int(pixels[1]), int(pixels[2]), int(pixels[3]),
            int(pixels[4]), int(pixels[5]), int(pixels[6]), int(pixels[7]));

        text_emit_str(head);
    }*/

    FILE* fp = fopen("mc-scr.data", "wb");
    if (!fp)
    {
        text_emit_str("\nerr: unable to open screenshot file for writing\n");
        return false;
    }

    size_t total_bytes = sizeof(pixels);
    const char* outbuf = reinterpret_cast<char*>(pixels);
    size_t total_written = 0;
    for (;;)
    {
        size_t bytes_written = fwrite(outbuf, 1, total_bytes - total_written, fp);
        total_written += bytes_written;
        outbuf += bytes_written;
        if (total_written == total_bytes)
            break;

        if (bytes_written == 0)
        {
            text_emit_str("Img write failed. Abandoning.\n");
            break;
        }

        text_emit_str("write to img continuing...\n");
    }

    fwrite(palRgb, 1, sizeof(palRgb), fp);

    fclose(fp);

    return true;
}
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------

bool handle_input(bool* outAnyInput)
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
            input_process_char(evt.text.text[0]);

            if (outAnyInput)
                *outAnyInput = true;

            break;

        case SDL_KEYDOWN:
            {
                if (outAnyInput)
                    *outAnyInput = true;

                const int keycode = evt.key.keysym.sym;
                const int scancode = evt.key.keysym.scancode;
                switch (keycode)
                {
                case SDLK_UP:
                case SDLK_DOWN:
                case SDLK_BACKSPACE:
                case SDLK_DELETE:
                case SDLK_RETURN:
                case SDLK_LEFT:
                case SDLK_RIGHT:
                    input_process_char(keycode);
                    if (input_has_complete_line())
                        eval_input();
                    break;

                case SDLK_F12:
                    cmd_screenshot(nullptr);
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

std::pair<bool,int> run_unit_tests(int argc, const char** argv)
{
    doctest::Context context;
    context.applyCommandLine(argc, argv);

    const int result = context.run();

    return {context.shouldExit(), result};
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

int main(int argc, const char** argv)
{
    {
        const auto& [should_exit, exitcode] = run_unit_tests(argc, argv);
        if (should_exit)
        {
            fprintf(stderr, "unit tests failed; aborting");
            return exitcode;
        }
    }

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

    gfx_set_palette(palette_get_lite());

    if (!lcd_init(text_get_background()))
        return 1;

    text_init();

    calc_init(text_emit_str);
    register_calc_cmd(cmd_big, "big", "", "switches to big text");
    register_calc_cmd(cmd_small, "small", "", "switches to small text");
    register_calc_cmd(cmd_bye, "bye", "", "closes the calc");

    text_emit_str(MCALC_WELCOME);
    text_emit_str("\n");
    input_reset_line();

#if 0
    {
        col8_t line[WIDTH];
        col8_t* p = line;
        for (int col = 0; col < 16; ++col)
        {
            for (int i=0; i<20; ++i, ++p)
                *p = col + 16;
        }
        for (int y=250; y<HEIGHT; ++y)
        {
            lcd_blit(line, 0, y, WIDTH, 1);
        }
    }
#endif

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
        handle_input(nullptr);

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



