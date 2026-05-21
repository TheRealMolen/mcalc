#include <SDL.h>
#include <algorithm>
#include <cstdio>

#include "drivers/font.h"
#include "drivers/lcd.h"
#include "drivers/text.h"

#include "libcalc/libcalc.h"


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
            //printf("textinput: char=%c\n", evt.text.text[0]);
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
    input_reset_line();

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



