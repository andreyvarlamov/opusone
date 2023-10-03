#include "opusone_common.h"
#include "opusone_math.h"
#include "opusone_linmath.h"

internal inline b32
IsSeparatingAxisTriBox(vec3 TestAxis, vec3 A, vec3 B, vec3 C, vec3 BoxExtents, vec3 *BoxAxes, f32 *Out_PenetrationDistance)
{
    f32 AProj = VecDot(A, TestAxis);
    f32 BProj = VecDot(B, TestAxis);
    f32 CProj = VecDot(C, TestAxis);
#if 1
    f32 MinTriProj = Min(Min(AProj, BProj), CProj);
    f32 MaxTriProj = Max(Max(AProj, BProj), CProj);
    f32 MaxBoxProj = (BoxExtents.E[0] * AbsF(VecDot(BoxAxes[0], TestAxis)) +
                      BoxExtents.E[1] * AbsF(VecDot(BoxAxes[1], TestAxis)) +
                      BoxExtents.E[2] * AbsF(VecDot(BoxAxes[2], TestAxis)));
    f32 MinBoxProj = -MaxBoxProj;
    if (MaxTriProj < MinBoxProj || MinTriProj > MaxBoxProj)
    {
        return true;
    }
    
    if (Out_PenetrationDistance) *Out_PenetrationDistance = Min(MaxBoxProj - MinTriProj, MaxTriProj - MinBoxProj);
    return false;
#else
    f32 BoxRProj = (BoxExtents.E[0] * AbsF(VecDot(BoxAxes[0], TestAxis)) +
                    BoxExtents.E[1] * AbsF(VecDot(BoxAxes[1], TestAxis)) +
                    BoxExtents.E[2] * AbsF(VecDot(BoxAxes[2], TestAxis)));
    f32 MaxTriVertProj = Max(-Max(Max(AProj, BProj), CProj), Min(Min(AProj, BProj), CProj));
    b32 Result = (MaxTriVertProj > BoxRProj);
    return Result;
#endif
}

internal inline b32
DoesBoxIntersectPlane(vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, vec3 PlaneNormal, f32 PlaneDistance, f32 *Out_PenetrationDistance)
{
    f32 BoxRProjTowardsPlane = (BoxExtents.E[0] * AbsF(VecDot(BoxAxes[0], PlaneNormal)) +
                                BoxExtents.E[1] * AbsF(VecDot(BoxAxes[1], PlaneNormal)) +
                                BoxExtents.E[2] * AbsF(VecDot(BoxAxes[2], PlaneNormal)));

    f32 PlaneProjFromBoxC = VecDot(PlaneNormal, BoxCenter) - PlaneDistance;

    if (AbsF(PlaneProjFromBoxC) <= BoxRProjTowardsPlane)
    {
        if (Out_PenetrationDistance) *Out_PenetrationDistance = BoxRProjTowardsPlane - PlaneProjFromBoxC;
        return true;
    }
    
    return false;
}

internal inline void
GetTriProjOnAxisMinMax(vec3 Axis, vec3 A, vec3 B, vec3 C, f32 *Out_Min, f32 *Out_Max)
{
    Assert(Out_Min);
    Assert(Out_Max);
    
    f32 AProj = VecDot(A, Axis);
    f32 BProj = VecDot(B, Axis);
    f32 CProj = VecDot(C, Axis);
    
    *Out_Min = Min(Min(AProj, BProj), CProj);
    *Out_Max = Max(Max(AProj, BProj), CProj);
}

internal inline void
GetBoxProjOnAxisMinMax(vec3 Axis, vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, f32 *Out_Min, f32 *Out_Max)
{
    Assert(Out_Min);
    Assert(Out_Max);

    f32 BoxCenterProj = VecDot(BoxCenter, Axis);
    f32 BoxRProj = (BoxExtents.E[0] * AbsF(VecDot(BoxAxes[0], Axis)) +
                    BoxExtents.E[1] * AbsF(VecDot(BoxAxes[1], Axis)) +
                    BoxExtents.E[2] * AbsF(VecDot(BoxAxes[2], Axis)));
    
    *Out_Max = BoxCenterProj + BoxRProj;
    *Out_Min = BoxCenterProj - BoxRProj;
}

internal inline f32
GetRangesOverlap(f32 AMin, f32 AMax, f32 BMin, f32 BMax, b32 *Out_BComesFirst)
{
    f32 AMaxBMin = AMax - BMin;
    f32 BMaxAMin = BMax - AMin;

    b32 BComesFirst = (BMaxAMin < AMaxBMin);
    
    if (Out_BComesFirst) *Out_BComesFirst = BComesFirst;    
    return (BComesFirst ? BMaxAMin : AMaxBMin);
}

internal inline b32
IsThereASeparatingAxisTriBox(vec3 TranslationDir, vec3 A, vec3 B, vec3 C, vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, vec3 *Out_CollisionNormal, f32 *Out_PenetrationDepth, f32 *Out_Overlaps = 0, u32 *Out_AI = 0)
{
    // NOTE: Translate triangle as conceptually moving AABB to origin
    // A -= BoxCenter;
    // B -= BoxCenter;
    // C -= BoxCenter;

    // NOTE: Compute edge vectors of triangle
    vec3 AB = B - A;
    vec3 BC = C - B;
    vec3 CA = A - C;

    // NOTE: Keep track of the axis of the shortest penetration depth, and the shortest penetration depth
    // In case there was no separating axis found, that will be the collision normal.
    // TODO: Maybe it's faster if we first do a fast check for any separating axis, and then calculate collision normal only if there was
    // no separating axis.
    f32 SmallestOverlap = FLT_MAX;
    vec3 SmallestOverlapAxis = {};
    u32 SmallestOverlapAxisIndex = 0;
    
    // NOTE: Test triangle's normal
    {    
        vec3 TestAxis = VecCross(AB, BC);

        if (VecDot(TranslationDir, TestAxis) > 0.0f)
        {
            // NOTE: If the translation is along triangle's normal, ignore collisions, as player is moving out of the
            // triangle collision anyways
            return true;
        }
        
        if (!IsZeroVector(TestAxis))
        {
            TestAxis = VecNormalize(TestAxis);

            f32 TriMin, TriMax;
            GetTriProjOnAxisMinMax(TestAxis, A, B, C, &TriMin, &TriMax);

            f32 BoxMin, BoxMax;
            GetBoxProjOnAxisMinMax(TestAxis, BoxCenter, BoxExtents, BoxAxes, &BoxMin, &BoxMax);

            b32 BoxIsInNegativeDirFromTri;
            f32 Overlap = GetRangesOverlap(TriMin, TriMax, BoxMin, BoxMax, &BoxIsInNegativeDirFromTri);

            if (Overlap < 0.0f)
            {
                // NOTE: The axis is separating
                return true;
            }

            f32 UnidirectionalOverlap = TriMax - BoxMin;

            if (Out_Overlaps) Out_Overlaps[0] = Overlap;

            if (Overlap < SmallestOverlap)
            {
                SmallestOverlap = Overlap;
                SmallestOverlapAxis = BoxIsInNegativeDirFromTri ? -TestAxis : TestAxis;
                SmallestOverlapAxisIndex = 0;
            }
        }
    }

    // NOTE: Test box's 3 normals
    for (u32 BoxAxisIndex = 0;
         BoxAxisIndex < 3;
         ++BoxAxisIndex)
    {
        vec3 TestAxis = BoxAxes[BoxAxisIndex];

        f32 TriMin, TriMax;
        GetTriProjOnAxisMinMax(TestAxis, A, B, C, &TriMin, &TriMax);

        f32 BoxMin, BoxMax;
        GetBoxProjOnAxisMinMax(TestAxis, BoxCenter, BoxExtents, BoxAxes, &BoxMin, &BoxMax);

        b32 BoxIsInNegativeDirFromTri;
        f32 Overlap = GetRangesOverlap(TriMin, TriMax, BoxMin, BoxMax, &BoxIsInNegativeDirFromTri);

        if (Overlap < 0.0f)
        {
            // NOTE: The axis is separating
            return true;
        }

        if (Out_Overlaps) Out_Overlaps[1+BoxAxisIndex] = Overlap;

        if (Overlap < SmallestOverlap)
        {
            SmallestOverlap = Overlap;
            SmallestOverlapAxis = BoxIsInNegativeDirFromTri ? -TestAxis : TestAxis;
            SmallestOverlapAxisIndex = 1+BoxAxisIndex;
        }
    }

    // NOTE: Test cross products between polyhedron edges (9 axis)
    vec3 Edges[] = { AB, BC, CA };
    for (u32 BoxAxisIndex = 0;
         BoxAxisIndex < 3;
         ++BoxAxisIndex)
    {
        for (u32 EdgeIndex = 0;
             EdgeIndex < 3;
             ++EdgeIndex)
        {
            vec3 TestAxis = VecCross(BoxAxes[BoxAxisIndex], Edges[EdgeIndex]);
            if (!IsZeroVector(TestAxis))
            {
                TestAxis = VecNormalize(TestAxis);

                // GetTriProjOnAxisMinMax(vec3 Axis, vec3 A, vec3 B, vec3 C, f32 *Out_Min, f32 *Out_Max);
                // GetBoxProjOnAxisMinMax(vec3 Axis, vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, f32 *Out_Min, f32 *Out_Max);
                // GetRangesOverlap(f32 AMin, f32 AMax, f32 BMin, f32 BMax, b32 *Out_BComesFirst);

                f32 TriMin, TriMax;
                GetTriProjOnAxisMinMax(TestAxis, A, B, C, &TriMin, &TriMax);

                f32 BoxMin, BoxMax;
                GetBoxProjOnAxisMinMax(TestAxis, BoxCenter, BoxExtents, BoxAxes, &BoxMin, &BoxMax);

                b32 BoxIsInNegativeDirFromTri;
                f32 Overlap = GetRangesOverlap(TriMin, TriMax, BoxMin, BoxMax, &BoxIsInNegativeDirFromTri);

                if (Overlap < 0.0f)
                {
                    // NOTE: The axis is separating
                    return true;
                }

                if (Out_Overlaps) Out_Overlaps[4+BoxAxisIndex*3+EdgeIndex] = Overlap;

                if (Overlap < SmallestOverlap)
                {
                    SmallestOverlap = Overlap;
                    SmallestOverlapAxis = BoxIsInNegativeDirFromTri ? -TestAxis : TestAxis;
                    SmallestOverlapAxisIndex = 4+BoxAxisIndex*3+EdgeIndex;
                }
            }
        }
    }

    //
    // NOTE: Did not find a separating axis, tri and box intersect, the axis of smallest penetration will be the collision normal
    //
    if (Out_CollisionNormal) *Out_CollisionNormal = SmallestOverlapAxis;
    if (Out_PenetrationDepth) *Out_PenetrationDepth = SmallestOverlap;
    if (Out_AI) *Out_AI = SmallestOverlapAxisIndex;

    return false;
}

internal inline b32
IntersectRayAABB(vec3 P, vec3 D, vec3 BoxCenter, vec3 BoxExtents, f32 *Out_TMin, vec3 *Out_Q)
{
    f32 TMin = 0.0f;
    f32 TMax = FLT_MAX;

    for (u32 AxisIndex = 0;
         AxisIndex < 3;
         ++AxisIndex)
    {
        f32 AMin = BoxCenter.E[AxisIndex] - BoxExtents.E[AxisIndex];
        f32 AMax = BoxCenter.E[AxisIndex] + BoxExtents.E[AxisIndex];
        if (AbsF(D.E[AxisIndex]) <= FLT_EPSILON)
        {
            // NOTE: Ray is parallel to slab. No hit if origin not within slab
            if (P.E[AxisIndex] < AMin || P.E[AxisIndex] > AMax)
            {
                return false;
            }
        }
        else
        {
            f32 OneOverD = 1.0f / D.E[AxisIndex];
            f32 T1 = (AMin - P.E[AxisIndex]) * OneOverD;
            f32 T2 = (AMax - P.E[AxisIndex]) * OneOverD;

            // NOTE:Make T1 intersection with near plane, T2 - with far plane
            if (T1 > T2)
            {
                f32 Temp = T1;
                T1 = T2;
                T2 = Temp;
            }

            // NOTE: Compute the intersection of slab intersection intervals
            if (T1 > TMin)
            {
                TMin = T1;
            }
            if (T2 < TMax)
            {
                TMax = T2;
            }

            if (TMin > TMax)
            {
                return false;
            }
        }
    }

    if (Out_TMin) *Out_TMin = TMin;
    if (Out_Q) *Out_Q = P + D * TMin;

    return true;
}

internal inline b32
IntersectRayTri(vec3 P, vec3 D, vec3 A, vec3 B, vec3 C, f32 *Out_U, f32 *Out_V, f32 *Out_W, f32 *Out_T)
{
    vec3 AB = B - A;
    vec3 AC = C - A;

    vec3 N = VecCross(AB, AC);

    // NOTE: Compute denominator d. If d <= 0, segment is parallel to or points away from triangle - exit early
    f32 Denominator = VecDot(-D, N);
    if (Denominator <= 0.0f)
    {
        return false;
    }

    // NOTE: Compute t - value of PQ at intersection with the plane of the triangle.
    // Delay dividing by d until intersection has been found to actually pierce the triangle.
    vec3 AP = P - A;
    f32 T = VecDot(AP, N);
    if (T < 0.0f)
    {
        return false;
    }

    // NOTE: Compute barycentric coordinate components and test if within bounds.
    vec3 E = VecCross(-D, AP);
    f32 V = VecDot(AC, E);
    if (V < 0.0f || V > Denominator)
    {
        return false;
    }
    f32 W = -VecDot(AB, E);
    if (W < 0.0f || V + W > Denominator)
    {
        return false;
    }

    if (Out_U || Out_V || Out_W || Out_T)
    {
        f32 OneOverD = 1.0f / Denominator;
        if (Out_T) *Out_T = T * OneOverD;

        if (Out_U || Out_V || Out_W)
        {
            V *= OneOverD;
            W *= OneOverD;
            if (Out_U) *Out_U = 1.0f - V - W;
            if (Out_V) *Out_V = V;
            if (Out_W) *Out_W = W;
        }
    }

    return true;
}
