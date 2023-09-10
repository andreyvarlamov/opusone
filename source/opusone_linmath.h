#ifndef OPUSONE_LINMATH_H
#define OPUSONE_LINMATH_H

#include "opusone_common.h"
#include "opusone_math.h"

// NOTE: All mat functions assume Column-Major representation [Column][Row]

// -------------------------------------------------------------------------------
// VECTOR 2 ----------------------------------------------------------------------
// -------------------------------------------------------------------------------

union vec2
{
    struct
    {
        f32 X, Y;
    };

    f32 E[2];
};

internal inline vec2
operator+(vec2 V0, vec2 V1)
{
    return vec2 { V0.X + V1.X, V0.Y + V1.Y };
}

internal inline vec2
operator-(vec2 V0, vec2 V1)
{
    return vec2 { V0.X - V1.X, V0.Y - V1.Y };
}

internal inline vec2
operator-(vec2 V0)
{
    return vec2 { -V0.X, -V0.Y };
}

internal inline vec2
operator*(vec2 V, f32 S)
{
    return vec2 { V.X * S, V.Y * S };
}

internal inline  vec2
operator*(f32 S, vec2 V)
{
    return vec2 { V.X * S, V.Y * S };
}

internal inline vec2
operator/(vec2 V, f32 S)
{
    return vec2 { V.X / S, V.Y / S };
}

internal inline vec2 &
operator+=(vec2 &V0, vec2 V1)
{
    V0 = V0 + V1;
    return V0;
}

internal inline vec2 &
operator-=(vec2 &V0, vec2 V1)
{
    V0 = V0 - V1;
    return V0;
}

internal inline vec2 &
operator*=(vec2 &V, f32 S)
{
    V = V * S;
    return V;
}

internal inline vec2 &
operator/=(vec2 &V, f32 S)
{
    V = V / S;
    return V;
}

internal inline f32
VecDot(vec2 V0, vec2 V1)
{
    return (V0.X * V1.X + V0.Y * V1.Y);
}

internal inline f32
VecLengthSq(vec2 V)
{
    return VecDot(V, V);
}

internal inline f32
VecLength(vec2 V)
{
    return SqrtF(VecLengthSq(V));
}

internal inline vec2
VecNormalize(vec2 V)
{
    f32 Length = VecLength(V);
    return ((Length != 0.0f) ? (V / Length) : V);
}

// -------------------------------------------------------------------------------
// VECTOR 3 ----------------------------------------------------------------------
// -------------------------------------------------------------------------------

union vec3
{
    struct
    {
        f32 X, Y, Z;
    };

    f32 E[3];
};


internal inline vec3
operator+(vec3 V0, vec3 V1)
{
    return vec3 { V0.X + V1.X, V0.Y + V1.Y, V0.Z + V1.Z };
}

internal inline vec3
operator-(vec3 V0, vec3 V1)
{
    return vec3 { V0.X - V1.X, V0.Y - V1.Y, V0.Z - V1.Z };
}

internal inline vec3
operator-(vec3 V0)
{
    return vec3 { -V0.X, -V0.Y, -V0.Z };
}

internal inline vec3
operator*(vec3 V, f32 S)
{
    return vec3 { V.X * S, V.Y * S, V.Z * S };
}

internal inline vec3
operator*(f32 S, vec3 V)
{
    return vec3 { V.X * S, V.Y * S, V.Z * S };
}

internal inline vec3
operator/(vec3 V, f32 S)
{
    return vec3 { V.X / S, V.Y / S, V.Z / S };
}

internal inline vec3 &
operator+=(vec3 &V0, vec3 V1)
{
    V0 = V0 + V1;
    return V0;
}

internal inline vec3 &
operator-=(vec3 &V0, vec3 V1)
{
    V0 = V0 - V1;
    return V0;
}

internal inline vec3 &
operator*=(vec3 &V, f32 S)
{
    V = V * S;
    return V;
}

internal inline vec3 &
operator/=(vec3 &V, f32 S)
{
    V = V / S;
    return V;
}

internal inline f32
VecDot(vec3 V0, vec3 V1)
{
    return (V0.X * V1.X + V0.Y * V1.Y + V0.Z * V1.Z);
}

internal inline f32
VecLengthSq(vec3 V)
{
    return VecDot(V, V);
}

internal inline f32
VecLength(vec3 V)
{
    return SqrtF(VecLengthSq(V));
}

internal inline vec3
VecNormalize(vec3 V)
{
    f32 Length = VecLength(V);
    return ((Length != 0.0f) ? (V / Length) : V);
}

internal inline vec3
VecCross(vec3 V0, vec3 V1)
{
    return vec3 { V0.Y * V1.Z - V0.Z * V1.Y,
        V0.Z * V1.X - V0.X * V1.Z,
        V0.X * V1.Y - V0.Y * V1.X };
}

internal inline f32
VecScalarTriple(vec3 A, vec3 B, vec3 C)
{
    return VecDot(A, VecCross(B, C));
}

internal inline b32
IsZeroVector(vec3 Vector)
{
    return ((Vector.X <= FLT_EPSILON) &&
            (Vector.Y <= FLT_EPSILON) &&
            (Vector.Z <= FLT_EPSILON));
}

internal inline vec3
VecSphericalToCartesian(f32 Theta, f32 Phi)
{
    // NOTE: Using coordinate system ZXY; Theta = Angle from axis Z in direction of X (CCW);
    // Phi = Angle from axis Y in direction ZX plane (CW, from top to down)
    vec3 Result = {};

    f32 ThetaRads = ToRadiansF(Theta);
    f32 PhiRads = ToRadiansF(Phi);

    Result.X = SinF(PhiRads) * SinF(ThetaRads);
    Result.Y = CosF(PhiRads);
    Result.Z = SinF(PhiRads) * CosF(ThetaRads);

    return Result;
}

// -------------------------------------------------------------------------------
// VECTOR 4 ----------------------------------------------------------------------
// -------------------------------------------------------------------------------

union vec4
{
    struct
    {
        f32 X, Y, Z, W;
    };

    f32 E[4];
};

internal inline vec4
operator+(vec4 V0, vec4 V1);

internal inline vec4
operator-(vec4 V0, vec4 V1);

internal inline vec4
operator-(vec4 V0);

internal inline vec4
operator*(vec4 V, f32 S);

internal inline vec4
operator*(f32 S, vec4 V);

internal inline vec4
operator/(vec4 V, f32 S);

internal inline vec4 &
operator+=(vec4 &V0, vec4 V1);

internal inline vec4 &
operator-=(vec4 &V0, vec4 V1);

internal inline vec4 &
operator*=(vec4 &V, f32 S);

internal inline vec4 &
operator/=(vec4 &V, f32 S);

// -------------------------------------------------------------------------------
// MATRIX 3 ----------------------------------------------------------------------
// -------------------------------------------------------------------------------

struct mat3
{
    f32 E[3][3];
};

internal inline mat3
Mat3Identity()
{
    mat3 Result = {};

    Result.E[0][0] = 1.0f;
    Result.E[1][1] = 1.0f;
    Result.E[2][2] = 1.0f;

    return Result;
}

internal inline mat3
Mat3Transpose(mat3 M)
{
    mat3 Result = {};

    for (u32 Column = 0; Column < 3; ++Column)
    {
        for (u32 Row = 0; Row < 3; ++Row)
        {
            Result.E[Column][Row] = M.E[Row][Column];
        }
    }

    return Result;
}

internal inline mat3
operator*(mat3 M0, mat3 M1)
{
    mat3 Result = {};

    for (i32 I = 0; I < 3; ++I)
    {
        for (i32 J = 0; J < 3; ++J)
        {
            Result.E[J][I] = (M0.E[0][I]*M1.E[J][0] +
                              M0.E[1][I]*M1.E[J][1] +
                              M0.E[2][I]*M1.E[J][2]);
        }
    }

    return Result;
}

internal inline vec3
operator*(mat3 M, vec3 V)
{
    vec3 Result = {};

    Result.X = M.E[0][0] * V.X + M.E[1][0] * V.Y + M.E[2][0] * V.Z;
    Result.Y = M.E[0][1] * V.X + M.E[1][1] * V.Y + M.E[2][1] * V.Z;
    Result.Z = M.E[0][2] * V.X + M.E[1][2] * V.Y + M.E[2][2] * V.Z;

    return Result;
}

internal inline mat3
Mat3FromCols(vec3 A, vec3 B, vec3 C)
{
    mat3 Result = {};

    Result.E[0][0] = A.X;
    Result.E[0][1] = A.Y;
    Result.E[0][2] = A.Z;

    Result.E[1][0] = B.X;
    Result.E[1][1] = B.Y;
    Result.E[1][2] = B.Z;

    Result.E[2][0] = C.X;
    Result.E[2][1] = C.Y;
    Result.E[2][2] = C.Z;

    return Result;
}

internal inline mat3
Mat3FromCols(vec3 *Cols)
{
    Assert(Cols);

    mat3 Result = {};

    Result.E[0][0] = Cols[0].X;
    Result.E[0][1] = Cols[0].Y;
    Result.E[0][2] = Cols[0].Z;

    Result.E[1][0] = Cols[1].X;
    Result.E[1][1] = Cols[1].Y;
    Result.E[1][2] = Cols[1].Z;

    Result.E[2][0] = Cols[2].X;
    Result.E[2][1] = Cols[2].Y;
    Result.E[2][2] = Cols[2].Z;

    return Result;
}

internal inline mat3
Mat3GetRotationAroundAxis(vec3 Axis, f32 Angle)
{
    f32 C = CosF(Angle);
    f32 S = SinF(Angle);
    
    vec3 Temp = (1.0f - C) * Axis;

    mat3 Result;

    Result.E[0][0] = C + Temp.E[0] * Axis.E[0];
    Result.E[0][1] = Temp.E[0] * Axis.E[1] + S * Axis.E[2];
    Result.E[0][2] = Temp.E[0] * Axis.E[2] - S * Axis.E[1];

    Result.E[1][0] = Temp.E[1] * Axis.E[0] - S * Axis.E[2];
    Result.E[1][1] = C + Temp.E[1] * Axis.E[1];
    Result.E[1][2] = Temp.E[1] * Axis.E[2] + S * Axis.E[0];

    Result.E[2][0] = Temp.E[2] * Axis.E[0] + S * Axis.E[1];
    Result.E[2][1] = Temp.E[2] * Axis.E[1] - S * Axis.E[0];
    Result.E[2][2] = C + Temp.E[2] * Axis.E[2];

    return Result;
}

internal inline vec3
Mat3GetCol(mat3 M, u32 ColumnIndex)
{
    Assert(ColumnIndex < 3);

    vec3 Result;

    for (u32 RowIndex = 0; RowIndex < 3; ++RowIndex)
    {
        Result.E[RowIndex] = M.E[ColumnIndex][RowIndex];
    }

    return Result;
}

internal inline void
Mat3GetCols(mat3 M, vec3 *Out_Cols)
{
    Assert(Out_Cols);

    for (u32 ColIndex = 0; ColIndex < 3; ++ColIndex)
    {
        for (u32 RowIndex = 0; RowIndex < 3; ++RowIndex)
        {
            Out_Cols[ColIndex].E[RowIndex] = M.E[ColIndex][RowIndex];
        }
    }
}

// -------------------------------------------------------------------------------
// MATRIX 4 ----------------------------------------------------------------------
// -------------------------------------------------------------------------------

struct mat4
{
    f32 E[4][4];
};

internal inline mat4
Mat4Identity()
{
    mat4 Result = {};

    Result.E[0][0] = 1.0f;
    Result.E[1][1] = 1.0f;
    Result.E[2][2] = 1.0f;
    Result.E[3][3] = 1.0f;

    return Result;
}

internal inline mat4
operator*(mat4 M0, mat4 M1)
{
    mat4 Result = {};

    for (i32 I = 0; I < 4; ++I)
    {
        for (i32 J = 0; J < 4; ++J)
        {
            Result.E[J][I] = (M0.E[0][I]*M1.E[J][0] +
                              M0.E[1][I]*M1.E[J][1] +
                              M0.E[2][I]*M1.E[J][2] +
                              M0.E[3][I]*M1.E[J][3]);
        }
    }

    return Result;
}

internal inline vec4
operator*(mat4 M, vec4 V)
{
    vec4 Result = {};

    Result.X = M.E[0][0] * V.X + M.E[1][0] * V.Y + M.E[2][0] * V.Z + M.E[3][0] * V.W;
    Result.Y = M.E[0][1] * V.X + M.E[1][1] * V.Y + M.E[2][1] * V.Z + M.E[3][1] * V.W;
    Result.Z = M.E[0][2] * V.X + M.E[1][2] * V.Y + M.E[2][2] * V.Z + M.E[3][2] * V.W;
    Result.W = M.E[0][2] * V.X + M.E[1][2] * V.Y + M.E[2][2] * V.Z + M.E[3][3] * V.W;

    return Result;
}


// ---------------------------------------------------------------------------------
// TRANSFORMS ----------------------------------------------------------------------
// ---------------------------------------------------------------------------------

mat4
GetViewMat(vec3 EyePosition, vec3 TargetPoint, vec3 WorldUp)
{
    // NOTE: General formula: https://registry.khronos.org/OpenGL-Refpages/gl2.1/xhtml/gluLookAt.xml
    // Translation optimization by dot product from GLM

    vec3 Front = VecNormalize(TargetPoint - EyePosition);
    vec3 Right = VecNormalize(VecCross(Front, VecNormalize(WorldUp)));
    vec3 Up = VecCross(Right, Front);

    mat4 Result = Mat4Identity();
    Result.E[0][0] =  Right.X;
    Result.E[0][1] =  Up.X;
    Result.E[0][2] = -Front.X;
    Result.E[1][0] =  Right.Y;
    Result.E[1][1] =  Up.Y;
    Result.E[1][2] = -Front.Y;
    Result.E[2][0] =  Right.Z;
    Result.E[2][1] =  Up.Z;
    Result.E[2][2] = -Front.Z;
    Result.E[3][0] = -VecDot(Right, EyePosition);
    Result.E[3][1] = -VecDot(Up, EyePosition);
    Result.E[3][2] =  VecDot(Front, EyePosition);

    return Result;
}

mat4
GetPerspecitveProjectionMat(f32 FovY_Degrees, f32 AspectRatio, f32 Near, f32 Far)
{
    // NOTE: http://www.songho.ca/opengl/gl_projectionmatrix.html
    mat4 Result = {};

    f32 HalfHeight = Near * TanF(ToRadiansF(FovY_Degrees) / 2.0f);
    f32 HalfWidth = HalfHeight * AspectRatio;

    Result.E[0][0] = Near / HalfWidth;
    Result.E[1][1] = Near / HalfHeight;
    Result.E[2][2] = -(Far + Near) / (Far - Near);
    Result.E[2][3] = -1.0f;
    Result.E[3][2] = -2.0f * Far * Near / (Far - Near);

    return Result;
}

#endif
