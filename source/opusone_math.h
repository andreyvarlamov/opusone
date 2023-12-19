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

inline f32
Square(f32 Value)
{
    return Value*Value;
}

inline b32
SolveQuadraticEquation(f32 A, f32 B, f32 C, f32 *Out_Root1, f32 *Out_Root2)
{
    f32 Determinant = B*B - 4.0f*A*C;

    if (Determinant < 0)
    {
        return false;
    }

    f32 SqrtD = SqrtF(Determinant);
    f32 Root1 = (-B - SqrtD) / (2.0f * A);
    f32 Root2 = (-B + SqrtD) / (2.0f * A);

    // Sort so X1 is smaller
    if (Root1 < Root2)
    {
        *Out_Root1 = Root1;
        *Out_Root2 = Root2;
    }
    else
    {
        *Out_Root1 = Root2;
        *Out_Root2 = Root1;
    }

    return true;
}

inline b32
GetLowestBoundedQuadraticRoot(f32 A, f32 B, f32 C, f32 MaxRoot, f32 *Out_Root)
{
    f32 Root1, Root2;
    if(SolveQuadraticEquation(A, B, C, &Root1, &Root2))
    {
        if (Root1 > 0 && Root1 < MaxRoot)
        {
            *Out_Root = Root1;
            return true;
        }

        // NOTE: It is possible that we want Root2, this can happen if Root1 < 0
        if (Root2 > 0 && Root2 < MaxRoot)
        {
            *Out_Root = Root2;
            return true;
        }

        // NOTE: No valid solutions
        return false;
    }
    else
    {
        return false;
    }
}

#endif
