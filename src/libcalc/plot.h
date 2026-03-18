#pragma once

#include "libcalc.h"
#include "parser.h"

#include <stdint.h>

//-------------------------------------------------------------------------------------------------

struct PlotAxis
{
    char Name[kMaxSymbolLength+1];
    double Lo = -1;
    double Hi = 1;
};

//-------------------------------------------------------------------------------------------------

struct FastAxis
{
    const PlotAxis& Axis;
    int StartI, EndI;
    int LoI, HiI;
    float Range;
    double IRange;
    float RangeRecip;
    float UnitsPerPix;

    FastAxis(const PlotAxis& axis, int startI, int endI)
        : Axis(axis)
        , StartI(startI)
        , EndI(endI)
        , LoI(startI < endI ? startI : endI)
        , HiI(endI > startI ? endI : startI)
        , Range(axis.Hi - axis.Lo)
        , IRange(endI - startI)
        , RangeRecip(1.0 / Range)
        , UnitsPerPix(Range / IRange)
    { /**/ }

    // chart coords are ints from 0 at left of axis to (HiI-1) at the right
    // screen coords are ints from 0 to screen buf size (MC_PLOT_*)

    double FromChart(int vi) const
    {
        return Axis.Lo + (vi * UnitsPerPix);
    }
    double FromScreen(int vi) const
    {
        return Axis.Lo + ((vi - LoI) * UnitsPerPix);
    }

    double ToScreen(double v) const
    {
        return (StartI + IRange * ((v - Axis.Lo) * RangeRecip));
    }
    double ToScreenClamped(double v) const
    {
        const double scr = ToScreen(v);
        if (scr < LoI)
            return LoI;
        if (scr > HiI)
            return HiI;
        return scr;
    }

    bool IsScreenValInArea(double scr) const
    {
        const int si = int(scr);
        return ((si >= LoI) && (si <= HiI));
    }

};

//-------------------------------------------------------------------------------------------------

bool draw_plot(const char* func_name, const PlotAxis* xAxis, const PlotAxis* yAxis, ParseCtx& ctx);

//-------------------------------------------------------------------------------------------------

