#include "libcalc.h"

#include "cmd.h"
#include "expr.h"
#include "format.h"
#include "funcs.h"
#include "parser.h"
#include "plot.h"
#include "symbols.h"

#include <cmath>
#include <cstring>
#include <iostream>

//-------------------------------------------------------------------------------------------------

calc_puts_func calc_puts_fn;

void calc_puts(const char* str)
{
    if (calc_puts_fn)
    {
        calc_puts_fn(str);
    }
}

//-------------------------------------------------------------------------------------------------

// assignment ::= "->" | "="
// f[x] assignment expression
// x assignment expression
// definition ::= symbol [lparen symbol rparen] assignment expression
bool parse_definition(ParseCtx& ctx)
{
    char name[kMaxSymbolLength+1];

    bool isFunction = false;
    char arg[kMaxSymbolLength+1];

    if (!expect_symbol(ctx, name))
        return false;

    if (accept(ctx, Token::LParen))
    {
        isFunction = true;

        if (!expect_symbol(ctx, arg))
            return false;
        if (!expect(ctx, Token::RParen))
            return false;
    }

    // we need to cache the pointer to the rest of the string now before we advance token
    // otherwise the function def will miss the first token
    const char* postAssignBuf = ctx.InBuffer + ctx.CurrIx;

    if (!accept(ctx, Token::Map) && !expect(ctx, Token::Equals))
        return false;

    if (isFunction)
    {
        // the remainder of the expression becomes the registered implementation of function <name>
        ParseCtx innerCtx {
            .InBuffer = postAssignBuf,
            .ResBuffer = ctx.ResBuffer,
            .ResBufferLen = ctx.ResBufferLen
        };
        if (!define_function(name, arg, innerCtx))
            return false;

        // we've eaten all the rest of the input
        ctx.NextToken = Token::Eof;
    }
    else
    {
        const double val = parse_expression(ctx);
        if (ctx.Error)
            return false;

        return define_value(name, val, ctx);
    }

    return true;
}

//-------------------------------------------------------------------------------------------------

// axis ::= expression "<" symbol "<" expression
bool parse_axis(ParseCtx& ctx, PlotAxis& axis)
{
    double lo = parse_expression(ctx);
    if (ctx.Error)
        return false;

    if (!expect(ctx, Token::LessThan))
        return false;
    if (!expect_symbol(ctx, axis.Name))
        return false;
    if (!expect(ctx, Token::LessThan))
        return false;

    double hi = parse_expression(ctx);
    if (ctx.Error)
        return false;

    axis.Lo = lo;
    axis.Hi = hi;
    return true;
}


// g f -pi<x<pi, -1<y<1
// cmd_graph ::= "g" symbol [axis ["," axis]]
bool cmd_graph_y(ParseCtx& ctx)
{
    char func_name[kMaxSymbolLength+1];
    if (!expect_symbol(ctx, func_name))
    {
        on_parse_error(ctx, "need user func name for y=f(x)");
        return false;
    }
    if (!is_user_func(func_name))
    {
        on_parse_error(ctx, "unknown user function");
        return false;
    }

    PlotAxis x { .Name = "x" };
    PlotAxis y { .Name = "y" };

    while (!peek(ctx, Token::Eof))
    {
        const int axisStartIx = ctx.CurrIx;

        PlotAxis axis;
        if (!parse_axis(ctx, axis))
            return false;

        if (strcmp(axis.Name, x.Name) == 0)
            x = axis;
        else if (strcmp(axis.Name, y.Name) == 0)
            y = axis;
        else
        {
            ctx.CurrIx = axisStartIx;
            on_parse_error(ctx, "unknown axis");
            return false;
        }

        if (!accept(ctx, Token::Comma))
            break;
    }

    if (!draw_plot(func_name, &x, &y, ctx))
        return false;

    return true;
}

//-------------------------------------------------------------------------------------------------

bool try_parse_command(ParseCtx& ctx)
{
    if (!peek(ctx, Token::Symbol))
        return false;

    const CommandDef* cmd = lookup_command(ctx.TokenSymbol);
    if (!cmd)
        return false;

    // eat the command name symbol
    expect(ctx, Token::Symbol);

    if (cmd->Func)
        return cmd->Func(ctx.InBuffer + ctx.CurrIx);
    if (cmd->PFunc)
        return cmd->PFunc(ctx);

    on_parse_error(ctx, "corrupt command");
    return false;
}


//-------------------------------------------------------------------------------------------------
#include <SDL.h>
extern SDL_Surface* gBackBuffer;
extern bool handle_input();
extern void render();
constexpr static double pi = 3.14159265358979323846264338327950288419716939937510;

static constexpr uint16_t COL_AMBER = 0xff0a;
static constexpr uint16_t COL_AQUA = 0xb7f5;

static constexpr uint16_t bgCol = 0x1862;
static constexpr uint16_t lineCol = COL_AMBER;


struct DampedPendulumSystem
{
    using real_t = float;

    real_t x = 0;
    real_t v = 1;
    real_t z = 0;
    
    real_t damp = 0.05;
    real_t omega = 0.8;

    void setParamA(double val)  { damp = real_t(val); }
    void setParamB(double val)  { omega = real_t(val); }

    real_t getX() const { return x; }
    real_t getY() const { return v; }
    real_t getPhi() const { return z; }

    void next(double dt)
    {
        z += real_t(dt) * omega;
        z = fmod(z, pi*2);

        v += dt * ((-damp * v) - sin(x) + sin(z));
        x += dt * v;

        x = fmod(x, pi*2);
        if (x <= -pi)
            x += pi*2;
        if (x > pi)
            x -= pi*2;
    }
};


// warmly darken an RGB565 colour by ~10%
uint16_t darken(uint16_t c)
{
    uint16_t r = c >> 11;
    uint16_t g = (c >> 5) & 0x3f;
    uint16_t b = c & 0x1f;

    r = (r * 15) / 16;
    g = (g * 29) / 32;
    b = (b * 14) / 16;

    return (r << 11) | (g << 5) | (b);
}

void darken(SDL_Surface* surf)
{
    uint16_t* pix = (uint16_t*)(surf->pixels);
    int stride = surf->pitch / sizeof(uint16_t);
    uint16_t* pixEnd = pix + (stride * surf->h);
    for (uint16_t* p = pix; p != pixEnd; ++p)
        *p = darken(*p);
}

float lerp(float a, float b, float t)
{
    return a + (b-a)*t;
}


class AnimRenderer
{
    static constexpr int IMGW = 320;
    static constexpr int IMGH = 320;

    static constexpr int BORDER = 4;

    AnimRenderer() = delete;
    AnimRenderer(const AnimRenderer&) = delete;
    AnimRenderer(AnimRenderer&&) = delete;
    AnimRenderer& operator=(const AnimRenderer&) = delete;
    AnimRenderer& operator=(AnimRenderer&&) = delete;

public:
    AnimRenderer(float minX, float maxX, float minY, float maxY, uint16_t bgCol)
        : mAxisX{ .Name = "x", .Lo = minX, .Hi = maxX }
        , mAxisY{ .Name = "y", .Lo = minY, .Hi = maxY }
        , mX( mAxisX, BORDER, IMGW - BORDER - 1)
        , mY( mAxisY, IMGW - BORDER - 1, BORDER)
    {
        mSurf = SDL_CreateRGBSurfaceWithFormat(0, IMGW, IMGH, 16, SDL_PIXELFORMAT_RGB565);
        if (!mSurf)
            return;

        mPix = (uint16_t*)(mSurf->pixels);
        mStride = mSurf->pitch / sizeof(uint16_t);

        fill(bgCol);
    }

    ~AnimRenderer()
    {
        SDL_FreeSurface(mSurf);
        mSurf = nullptr;
    }

    void safePlot(int x, int y, uint16_t col)
    {
        if (x < 0 || x >= mStride)
            return;
        if (y < 0 || y >= IMGH)
            return;

        mPix[y * mStride + x] = col;
    };

    void fill(uint16_t col)
    {
        uint16_t* pixEnd = mPix + (mStride * mSurf->h);
        for (uint16_t* p = mPix; p != pixEnd; ++p)
            *p = col;
    }
    
    void darken()
    {
        ::darken(mSurf);
    }

    void blit(SDL_Surface* dst)
    {
        SDL_BlitSurface(mSurf, nullptr, dst, nullptr);
        render();
    }

    inline int x(double realX) const { return int(mX.ToScreen(realX)); }
    inline int y(double realY) const { return int(mY.ToScreen(realY)); }

private:
    SDL_Surface* mSurf = nullptr;
    const PlotAxis mAxisX, mAxisY;
    const FastAxis mX, mY;
    uint16_t* mPix = nullptr;
    int mStride = IMGW;
};



template<typename SystemType>
bool cmd_anim_diff(ParseCtx& ctx)
{
    AnimRenderer rndr(-3.5, 3.5, -4.5, 4.5, bgCol);

    SystemType s;
    if (!peek(ctx, Token::Eof))
        s.setParamA(parse_expression(ctx));
    if (!peek(ctx, Token::Eof))
        s.setParamB(parse_expression(ctx));

    double step = 0.00001;
    double t = 0;
    for (;;)
    {
        for (int i = 0; i < 1000000; ++i, t += step)
        {
            s.next(step);

            const double x = s.getX();
            const double y = s.getY();

            const double xi = rndr.x(x);
            const double yi = rndr.y(y);

            rndr.safePlot(xi, yi, lineCol);
        }
    
        rndr.blit(gBackBuffer);
        rndr.darken();

        if (!handle_input())
            break;
    }

    return true;
}


template<typename SystemType>
bool cmd_anim_poincare([[maybe_unused]] ParseCtx& ctx)
{
    AnimRenderer rndr(-3.5, 3.5, -4.5, 4.5, bgCol);

    SystemType s;
    if (!peek(ctx, Token::Eof))
        s.setParamA(parse_expression(ctx));
    if (!peek(ctx, Token::Eof))
        s.setParamB(parse_expression(ctx));

    double step = 0.001;
    constexpr double slice = pi/2;
    for (int frame = 0; /**/; ++frame)
    {
        for (int i = 0; i < 5000000; ++i)
        {
            SystemType o = s;

            s.next(step);

            if (o.getPhi() < slice && s.getPhi() >= slice)
            {
                o.next(slice - o.getPhi());

                const double x = o.getX();
                const double y = o.getY();

                const double xi = rndr.x(x);
                const double yi = rndr.y(y);

                rndr.safePlot(xi, yi, lineCol);
            }
        }

        rndr.blit(gBackBuffer);

        if ((frame & 3) == 0)
            rndr.darken();

        if (!handle_input())
            break;
    }

    return true;
}

//-------------------------------------------------------------------------------------------------

void calc_init(calc_puts_func puts_func)
{
    calc_puts_fn = puts_func;

    init_commands();

    register_calc_cmd(cmd_graph_y, "g", "g fn [lo<x<hi] [, lo<y<hi]", "graph of y=fn(x)");
    register_calc_cmd(cmd_anim_diff<DampedPendulumSystem>, "d", "d", "draw an animated diff eqn");
    register_calc_cmd(cmd_anim_poincare<DampedPendulumSystem>, "p", "p", "draw an animated poincare...\n slice of a diff eqn");
}

//-------------------------------------------------------------------------------------------------

bool calc_eval(const char* expr, char* resBuffer, int resBufferLen)
{
    if (!resBuffer)
        return false;
    *resBuffer = 0;

    ParseCtx parseCtx { .InBuffer=expr, .ResBuffer=resBuffer, .ResBufferLen=resBufferLen };
    advance_token(parseCtx);

    // scan the expression to see if it's something unusual
    const bool isDefinition = (strchr(expr, '=') != nullptr) || (strstr(expr, "->") != nullptr);

    bool shouldPrintResult = false;
    double result = 0.0;

    if (isDefinition && parse_definition(parseCtx))
    {
        strcpy(resBuffer, "  ok.");
    }
    else if (try_parse_command(parseCtx))
    {
        // commands are expected to manage their own feedback
    }
    else
    {
        result = parse_expression(parseCtx);
        shouldPrintResult = !parseCtx.Error;
    }

    if (!accept(parseCtx, Token::Eof))
        on_parse_error(parseCtx, "trailing nonsense");
    
    if (shouldPrintResult)
    {
        strcpy(resBuffer, "  = ");
        const int introLen = strlen(resBuffer);
        dtostr_human(result, resBuffer + introLen, resBufferLen - introLen);
    }

    return !parseCtx.Error;
}



