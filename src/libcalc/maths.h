#pragma once

#include <cmath>

//-------------------------------------------------------------------------------------------------

// if val can be called a positive integer, update it with its factorial
// otherwise return false
bool compute_factorial(double& val);

double sinc(double v);

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------

constexpr static double pi = 3.14159265358979323846264338327950288419716939937510;

inline float lerp(float a, float b, float t)
{
    return a + (b-a)*t;
}

inline float signum(float x)
{
    if (x > 0.f)
        return 1;
    if (x < 0.f)
        return -1;
    return 0;
}

// return r in range [0,2pi)
inline float clampRads(float r)
{
    r = fmod(r, pi*2);
    if (r < 0)
        r += pi*2;

    return r;
}

// return r in range (-pi,pi]
inline float clampRadsSym(float r)
{
    r = fmod(r, pi*2);
    if (r <= -pi)
        r += pi*2;
    if (r > pi)
        r -= pi*2;

    return r;
}

//-------------------------------------------------------------------------------------------------

