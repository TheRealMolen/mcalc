#include "chaos.h"

#include "animrender.h"
#include "cmd.h"
#include "expr.h"
#include "maths.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

// some chaotic systems from Elegant Chaos by Julien Clinton Sprott

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
        v += dt * ((-damp * v) - sin(x) + sin(z));
        x += dt * v;

        z = clampRads(z);
        x = clampRadsSym(x);
    }
};


struct ForcedVdPolOscillator
{
    using real_t = float;

    real_t x = 1;
    real_t v = 0.1;
    real_t z = 0;
    
    real_t force = 0.5;
    real_t omega = 0.1;

    void setParamA(double val)  { force = real_t(val); }
    void setParamB(double val)  { omega = real_t(val); }

    real_t getX() const { return x; }
    real_t getY() const { return v; }
    real_t getPhi() const { return z; }

    void next(double dt)
    {
        z += real_t(dt) * omega;
        v += dt * ((force * sin(z)) - x - ((x*x - 1) * v));
        x += dt * v;

        z = clampRads(z);
        x = clampRadsSym(x);
    }
};


struct SignumSystem
{
    using real_t = float;

    real_t x = 1;
    real_t v = 0.1;
    real_t z = 0;
    
    void setParamA(double val)  { x = real_t(val); }
    void setParamB(double val)  { v = real_t(val); }

    real_t getX() const { return x * 0.6f; }
    real_t getY() const { return v; }
    real_t getPhi() const { return z; }

    void next(double dt)
    {
        z += real_t(dt);
        v += dt * (sin(z) - signum(x));
//        v += dt * (sin(z) - tanh(x*5000));
        x += dt * v;

        z = clampRads(z);
    }
};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

template<typename SystemType>
bool cmd_anim_diff(ParseCtx& ctx)
{
    AnimRenderer rndr(-3.5, 3.5, -4.5, 4.5);

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

            rndr.safePlot(xi, yi);
        }
    
        rndr.blit();
        rndr.darken();

        if (rndr.check_for_break())
            break;
    }

    return true;
}

//-------------------------------------------------------------------------------------------------

template<typename SystemType>
bool cmd_anim_poincare(ParseCtx& ctx)
{
    AnimRenderer rndr(-3.5, 3.5, -4.5, 4.5);

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

                rndr.safePlot(xi, yi);
            }
        }

        rndr.blit();

        if ((frame & 3) == 0)
            rndr.darken();

        if (rndr.check_for_break())
            break;
    }

    return true;
}

//-------------------------------------------------------------------------------------------------

void register_chaos_commands()
{
    register_calc_cmd(cmd_anim_diff<DampedPendulumSystem>, "da", "d", "draw an animated diff eqn");
    register_calc_cmd(cmd_anim_poincare<DampedPendulumSystem>, "pa", "p", "draw an animated poincare...\n slice of a diff eqn");
    //register_calc_cmd(cmd_anim_diff<ForcedVdPolOscillator>, "d", "d", "draw an animated diff eqn");
    //register_calc_cmd(cmd_anim_poincare<ForcedVdPolOscillator>, "p", "p", "draw an animated poincare...\n slice of a diff eqn");
    register_calc_cmd(cmd_anim_diff<SignumSystem>, "d", "d", "draw an animated diff eqn");
    register_calc_cmd(cmd_anim_poincare<SignumSystem>, "p", "p", "draw an animated poincare...\n slice of a diff eqn");
}

//-------------------------------------------------------------------------------------------------

