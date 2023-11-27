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
ToDegreesF(f32 Radians)
{
    return (Radians / PI32 * 180.0f);
}

internal inline f32
ArcSinF(f32 Value)
{
    return asinf(Value);
}

internal inline f32
ArcCosF(f32 Value)
{
    return acosf(Value);
}

internal inline f32
ClampF(f32 Value, f32 Min, f32 Max)
{
    if (Value < Min) return Min;
    if (Value > Max) return Max;
    return Value;
}

inline b32
SolveQuadraticEquation(f32 A, f32 B, f32 C, f32 MaxRoot, f32 *Out_Root)
{
    f32 Determinant = B*B - 4.0f*A*C;

    if (Determinant < 0)
    {
        return false;
    }

    f32 SqrtD = SqrtF(Determinant);
    f32 X1 = (-B - SqrtD) / (2.0f * A);
    f32 X2 = (-B + SqrtD) / (2.0f * A);

    // Sort so X1 is smaller
    if (X1 > X2)
    {
        f32 Temp = X2;
        X2 = X1;
        X1 = Temp;
    }

    if (X1 > 0 && X1 < MaxRoot)
    {
        *Out_Root = X1;
        return true;
    }

    if (X2 > 0 && X2 < MaxRoot)
    {
        *Out_Root = X2;
        return true;
    }

    return false;
}

#endif
