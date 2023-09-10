#ifndef OPUSONE_MATH_H
#define OPUSONE_MATH_H

#include "opusone_common.h"

#include <cmath>
#include <cfloat>

#define PI32 3.141592653f

internal inline f32
AbsF(f32 Value)
{
    return fabs(Value);
}

internal inline f32
SqrtF(f32 Value)
{
    return sqrtf(Value);
}

internal inline f32
SinF(f32 Value)
{
    return sinf(Value);
}

internal inline f32
CosF(f32 Value)
{
    return cosf(Value);
}

internal inline f32
TanF(f32 Value)
{
    return tanf(Value);
}

internal inline f32
ToRadiansF(f32 Degrees)
{
    return (Degrees * PI32 / 180.0f);
}

internal inline f32
ArcSinF(f32 Value)
{
    return asinf(Value);
}

internal inline f32
ClampF(f32 Value, f32 Min, f32 Max)
{
    if (Value < Min) return Min;
    if (Value > Max) return Max;
    return Value;
}

#endif
