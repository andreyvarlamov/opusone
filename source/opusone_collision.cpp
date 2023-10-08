#include "opusone_collision.h"

#include "opusone_common.h"
#include "opusone_linmath.h"

b32
ValidateEdgesUnique(memory_arena *TransientArena, edge *Edges, u32 EdgeCount)
{
    edge *EdgeScratch = MemoryArena_PushArray(TransientArena, EdgeCount, edge);
    u32 AddedToScratch = 0;

    for (u32 EdgeIndex = 0;
         EdgeIndex < EdgeCount;
         ++EdgeIndex)
    {
        for (u32 InnerIndex = 0;
             InnerIndex < AddedToScratch;
             ++InnerIndex)
        {
            if ((Edges[EdgeIndex].A == EdgeScratch[InnerIndex].A && Edges[EdgeIndex].B == EdgeScratch[InnerIndex].B) ||
                (Edges[EdgeIndex].A == EdgeScratch[InnerIndex].B && Edges[EdgeIndex].B == EdgeScratch[InnerIndex].A))
            {
                return false;
            }
        }

        EdgeScratch[AddedToScratch++] = Edges[EdgeIndex];
    }

    return true;
}

void
ComputePolyhedronFromVertices(memory_arena *Arena, memory_arena *TransientArena,
                              vec3 *ImportedVertices, u32 VertexCount, i32 *ImportedIndices, u32 IndexCount,
                              polyhedron *Out_Polyhedron)
{
    Assert(Arena);
    Assert(ImportedVertices);
    Assert(VertexCount > 0);
    Assert(ImportedIndices);
    Assert(IndexCount > 0);
    Assert(IndexCount % 3 == 0);
    Assert(Out_Polyhedron);

    MemoryArena_Freeze(TransientArena);
    
    polyhedron *Polyhedron = Out_Polyhedron;
    *Polyhedron = {};

    //
    // NOTE: 1. Deduplicate vertices and save into polyhedron vertex storage.
    //
    Polyhedron->Vertices = MemoryArena_PushArray(Arena, VertexCount, vec3);
    u32 UniqueVertexCount = 0;

    // NOTE: Store the pointers to polyhedron vertices in the original order (and duplication) to be able to use ImportedIndices.
    vec3 **OrderedVertexPtrs = MemoryArena_PushArray(TransientArena, VertexCount, vec3 *);

    for (u32 VertexIndex = 0;
         VertexIndex < VertexCount;
         ++VertexIndex)
    {
        vec3 *ImportedVertex = ImportedVertices + VertexIndex;

        b32 DuplicateFound = false;
        for (u32 UniqueVertexIndex = 0;
             UniqueVertexIndex < UniqueVertexCount;
             ++UniqueVertexIndex)
        {
            vec3 *PolyhedronVertex = Polyhedron->Vertices + UniqueVertexIndex;
            
            if (AreVecEqual(*ImportedVertex, *PolyhedronVertex))
            {
                OrderedVertexPtrs[VertexIndex] = PolyhedronVertex;
                DuplicateFound = true;
                break;
            }
        }

        if (!DuplicateFound)
        {
            vec3 *PolyhedronVertex = Polyhedron->Vertices + UniqueVertexCount++;
            *PolyhedronVertex = *ImportedVertex;
            OrderedVertexPtrs[VertexIndex] = PolyhedronVertex;
        }
    }
    
    Assert(UniqueVertexCount <= VertexCount);
    Polyhedron->VertexCount = UniqueVertexCount;
    MemoryArena_ResizePreviousPushArray(Arena, Polyhedron->VertexCount, vec3);

    u32 TriangleCount = IndexCount / 3;

    //
    // NOTE: 2. Process each triangle to make a table of unique normals and correspondent edges.
    //
    struct temp_edge
    {
        vec3 *PointA;
        vec3 *PointB;

        b32 IsInnerEdge;

        edge *PolyhedronEdgePtr;

        temp_edge *Next;
    };

    vec3 *UniqueNormals = MemoryArena_PushArray(TransientArena, TriangleCount, vec3);
    temp_edge *TempEdgeHeads = MemoryArena_PushArray(TransientArena, TriangleCount, temp_edge);
    i32 *EdgePerNormalCounts = MemoryArena_PushArray(TransientArena, TriangleCount, i32);

    u32 UniqueNormalCount = 0;
    for (u32 TriangleIndex = 0;
         TriangleIndex < TriangleCount;
         ++TriangleIndex)
    {
        vec3 *A = OrderedVertexPtrs[ImportedIndices[TriangleIndex*3+0]];
        vec3 *B = OrderedVertexPtrs[ImportedIndices[TriangleIndex*3+1]];
        vec3 *C = OrderedVertexPtrs[ImportedIndices[TriangleIndex*3+2]];

        // NOTE: Should never have a degenerate triangle at this point (if so -- something is wrong with import).
        Assert(!AreVecEqual(*A, *B) && !AreVecEqual(*B, *C) && !AreVecEqual(*A, *C));
        
        vec3 Normal = VecNormalize(VecCross(*B-*A, *C-*B));

        temp_edge TriangleEdges[3] = { { A, B }, { B, C }, { C, A } };
            
        u32 SearchTriangleIndex;
        for (SearchTriangleIndex = 0;
             SearchTriangleIndex < UniqueNormalCount;
             ++SearchTriangleIndex)
        {
            if (AreVecEqual(UniqueNormals[SearchTriangleIndex], Normal))
            {
                break;
            }
        }

        temp_edge *TempEdgeHead = TempEdgeHeads + SearchTriangleIndex;
        if (SearchTriangleIndex == UniqueNormalCount)
        {
            // NOTE: Duplicate normal wasn't found, initialize new slot.
            *TempEdgeHead = {};
            EdgePerNormalCounts[SearchTriangleIndex] = 0;
            UniqueNormals[SearchTriangleIndex] = Normal;
            ++UniqueNormalCount;
        }

        temp_edge *TempEdgeNode = TempEdgeHead;
        temp_edge *TempEdgeTail = TempEdgeHead;
        while (TempEdgeNode)
        {
            if (!TempEdgeNode->PointA)
            {
                break;
            }
            
            for (u32 TriangleEdgeIndex = 0;
                 TriangleEdgeIndex < 3;
                 ++TriangleEdgeIndex)
            {
                if ((TriangleEdges[TriangleEdgeIndex].PointA == TempEdgeNode->PointA &&
                     TriangleEdges[TriangleEdgeIndex].PointB == TempEdgeNode->PointB) ||
                    (TriangleEdges[TriangleEdgeIndex].PointA == TempEdgeNode->PointB &&
                     TriangleEdges[TriangleEdgeIndex].PointB == TempEdgeNode->PointA))
                {
                    TriangleEdges[TriangleEdgeIndex].IsInnerEdge = true;
                    TempEdgeNode->IsInnerEdge = true;
                    --EdgePerNormalCounts[SearchTriangleIndex];
                    break;
                }
            }

            TempEdgeTail = TempEdgeNode;
            TempEdgeNode = TempEdgeNode->Next;
        }

        for (u32 TriangleEdgeIndex = 0;
             TriangleEdgeIndex < 3;
             ++TriangleEdgeIndex)
        {
            if (!TempEdgeTail->PointA)
            {
                // NOTE: If the tail is not populated, it's the first edge being added in that slot.
                // Just copy into the tail. That'll be the only node in the linked list.
                *TempEdgeTail = TriangleEdges[TriangleEdgeIndex];
                if (!TempEdgeTail->IsInnerEdge)
                {
                    ++EdgePerNormalCounts[SearchTriangleIndex];
                }
                
                continue;
            }

            temp_edge *NewNode = MemoryArena_PushStruct(TransientArena, temp_edge);
            *NewNode = TriangleEdges[TriangleEdgeIndex];
            if (!NewNode->IsInnerEdge)
            {
                ++EdgePerNormalCounts[SearchTriangleIndex];
            }

            TempEdgeTail->Next = NewNode;
            TempEdgeTail = NewNode;
        }
    }

    //
    // NOTE: 3. Copy (polyhedron) unique edges into polyhedron storage.
    // Save pointers to the polyhedron edge storage in the temp edges, to preserve the link between polygons and edges.
    //
    Polyhedron->Edges = MemoryArena_PushArray(Arena, TriangleCount * 3, edge);

    u32 UniqueEdgeCount = 0;
    for (u32 UniqueNormalIndex = 0;
         UniqueNormalIndex < UniqueNormalCount-1;
         ++UniqueNormalIndex)
    {
        temp_edge *TempEdgeNode = TempEdgeHeads + UniqueNormalIndex;
        while (TempEdgeNode)
        {
            if (!TempEdgeNode->IsInnerEdge && !TempEdgeNode->PolyhedronEdgePtr)
            {
                edge *PolyhedronEdge = Polyhedron->Edges + UniqueEdgeCount++;
                PolyhedronEdge->A = TempEdgeNode->PointA;
                PolyhedronEdge->B = TempEdgeNode->PointB;
                PolyhedronEdge->AB = *PolyhedronEdge->B - *PolyhedronEdge->A;
                TempEdgeNode->PolyhedronEdgePtr = PolyhedronEdge;
                
                for (u32 NormalInnerIndex = UniqueNormalIndex+1;
                     NormalInnerIndex <  UniqueNormalCount;
                     ++NormalInnerIndex)
                {
                    temp_edge *TestTempEdgeNode = TempEdgeHeads + NormalInnerIndex;
                    while (TestTempEdgeNode)
                    {
                        if ((TempEdgeNode->PointA == TestTempEdgeNode->PointA && TempEdgeNode->PointB == TestTempEdgeNode->PointB) ||
                            (TempEdgeNode->PointA == TestTempEdgeNode->PointB && TempEdgeNode->PointB == TestTempEdgeNode->PointA))
                        {
                            TestTempEdgeNode->PolyhedronEdgePtr = PolyhedronEdge;
                        }
                        
                        TestTempEdgeNode = TestTempEdgeNode->Next;
                    }
                }
            }

            TempEdgeNode = TempEdgeNode->Next;
        }
    }

    // Assert(ValidateEdgesUnique(TransientArena, Polyhedron->Edges, UniqueEdgeCount));

    Polyhedron->EdgeCount = UniqueEdgeCount;
    MemoryArena_ResizePreviousPushArray(Arena, Polyhedron->EdgeCount, edge);

    Polyhedron->FaceCount = UniqueNormalCount;
    Polyhedron->Faces = MemoryArena_PushArray(Arena, Polyhedron->FaceCount, polygon);

    //
    // NOTE: 4. Process polyhedron faces saving polygon plane information, pointers to edges in polyhedron storage,
    // pointers to vertices in polyhedron storage.
    //
    vec3 *Normal = UniqueNormals;
    temp_edge *TempEdgeHead = TempEdgeHeads;
    i32 *EdgePerNormalCount = EdgePerNormalCounts;
    
    polygon *Face = Polyhedron->Faces;
    
    for (u32 FaceIndex = 0;
         FaceIndex < Polyhedron->FaceCount;
         ++FaceIndex, ++Normal, ++TempEdgeHead, ++EdgePerNormalCount, ++Face)
    {
        *Face = {};

        Face->Plane.Normal = *Normal;
        Face->Plane.Distance = VecDot(*Normal, *TempEdgeHead->PointA);

        Face->EdgeCount = *EdgePerNormalCount;
        Face->Edges = MemoryArena_PushArray(Arena, Face->EdgeCount, edge *);
        Face->Vertices = MemoryArena_PushArray(Arena, Face->EdgeCount * 2, vec3 *);
        u32 VerticesAdded = 0;

        temp_edge *TempEdgeNode = TempEdgeHead;
        u32 FaceEdgeIndex = 0;
        while (TempEdgeNode)
        {
            if (!TempEdgeNode->IsInnerEdge)
            {
                Face->Edges[FaceEdgeIndex++] = TempEdgeNode->PolyhedronEdgePtr;

                b32 FoundA = false;
                b32 FoundB = false;
                
                for (u32 VertexIndex = 0;
                     VertexIndex < VerticesAdded;
                     ++VertexIndex)
                {
                    if (Face->Vertices[VertexIndex] == TempEdgeNode->PointA)
                    {
                        FoundA = true;
                    }

                    if (Face->Vertices[VertexIndex] == TempEdgeNode->PointB)
                    {
                        FoundB = true;
                    }

                    if (FoundA && FoundB)
                    {
                        break;
                    }
                }
                
                if (!FoundA)
                {
                    Face->Vertices[VerticesAdded++] = TempEdgeNode->PointA;
                }
                
                if (!FoundB)
                {
                    Face->Vertices[VerticesAdded++] = TempEdgeNode->PointB;
                }
            }
            
            TempEdgeNode = TempEdgeNode->Next;
        }

        Face->VertexCount = VerticesAdded;
        MemoryArena_ResizePreviousPushArray(Arena, Face->VertexCount, vec3 *);
    }

    MemoryArena_Unfreeze(TransientArena);
}

inline void
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

inline void
GetPolyhedronProjOnAxisMinMax(vec3 TestAxis, vec3 *Vertices, u32 VertexCount, f32 *Out_Min, f32 *Out_Max)
{
    Assert(Out_Min);
    Assert(Out_Max);
    
    f32 MinProj = FLT_MAX;
    f32 MaxProj = -FLT_MAX;
    for (u32 VertexIndex = 0;
         VertexIndex < VertexCount;
         ++VertexIndex)
    {
        f32 Proj = VecDot(TestAxis, Vertices[VertexIndex]);

        if (Proj < MinProj)
        {
            MinProj = Proj;
        }

        if (Proj > MaxProj)
        {
            MaxProj = Proj;
        }
    }

    *Out_Min = MinProj;
    *Out_Max = MaxProj;
}

inline f32
GetRangesOverlap(f32 AMin, f32 AMax, f32 BMin, f32 BMax, b32 *Out_BComesFirst)
{
    f32 AMaxBMin = AMax - BMin;
    f32 BMaxAMin = BMax - AMin;

    b32 BComesFirst = (BMaxAMin < AMaxBMin);
    
    if (Out_BComesFirst) *Out_BComesFirst = BComesFirst;    
    return (BComesFirst ? BMaxAMin : AMaxBMin);
}

b32
IsSeparatedBoxPolyhedron(polyhedron *Polyhedron, box *Box, vec3 *Out_SmallestOverlapAxis, f32 *Out_SmallestOverlap)
{
    //
    // NOTE: 1. Check box face normals
    //
    for (u32 BoxAxisIndex = 0;
         BoxAxisIndex < 3;
         ++BoxAxisIndex)
    {
        vec3 TestAxis = Box->Axes[BoxAxisIndex];

        f32 BoxMinProj, BoxMaxProj;
        GetBoxProjOnAxisMinMax(TestAxis, Box->Center, Box->Extents, Box->Axes, &BoxMinProj, &BoxMaxProj);
        f32 PolyhedronMinProj, PolyhedronMaxProj;
        GetPolyhedronProjOnAxisMinMax(TestAxis, Polyhedron->Vertices, Polyhedron->VertexCount, &PolyhedronMinProj, &PolyhedronMaxProj);

        b32 IsBoxInFront;
        f32 Overlap = GetRangesOverlap(BoxMinProj, BoxMaxProj, PolyhedronMinProj, PolyhedronMaxProj, &IsBoxInFront);

        if (Overlap < 0.0f)
        {
            return true;
        }
    }

    //
    // NOTE: 2. Check polyhedron face normals
    //
    for (u32 FaceIndex = 0;
         FaceIndex < Polyhedron->FaceCount;
         ++FaceIndex)
    {
    }

    //
    // NOTE: 3. Check cross products of edges
    //
    for (u32 BoxAxisIndex = 0;
         BoxAxisIndex < 3;
         ++BoxAxisIndex)
    {
        for (u32 PolyhedronEdgeIndex = 0;
             PolyhedronEdgeIndex < Polyhedron->EdgeCount;
             ++PolyhedronEdgeIndex)
        {
            
        }
    }

    return false;
}

inline void
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

b32
IsThereASeparatingAxisTriBox(vec3 A, vec3 B, vec3 C, vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, vec3 *Out_SmallestOverlapAxis, f32 *Out_SmallestOverlap, b32 *Out_OverlapAxisIsTriNormal)
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
    b32 OverlapAxisIsTriNormal = false;

    // NOTE: Test triangle's normal
    {    
        vec3 TestAxis = VecNormalize(VecCross(AB, BC));
        
        if (!IsZeroVector(TestAxis))
        {
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

            if (Overlap < SmallestOverlap)
            {
                SmallestOverlap = Overlap;
                SmallestOverlapAxis = BoxIsInNegativeDirFromTri ? -TestAxis : TestAxis;
                SmallestOverlapAxisIndex = 0;
                OverlapAxisIsTriNormal = true;
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

        if (Overlap < SmallestOverlap)
        {
            SmallestOverlap = Overlap;
            SmallestOverlapAxis = BoxIsInNegativeDirFromTri ? -TestAxis : TestAxis;
            SmallestOverlapAxisIndex = 1+BoxAxisIndex;
            OverlapAxisIsTriNormal = false;
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

                if (Overlap < SmallestOverlap)
                {
                    SmallestOverlap = Overlap;
                    SmallestOverlapAxis = BoxIsInNegativeDirFromTri ? -TestAxis : TestAxis;
                    SmallestOverlapAxisIndex = 4+BoxAxisIndex*3+EdgeIndex;
                    OverlapAxisIsTriNormal = false;
                }
            }
        }
    }

    //
    // NOTE: Did not find a separating axis, tri and box intersect, the axis of smallest penetration will be the collision normal
    //
    if (Out_SmallestOverlapAxis) *Out_SmallestOverlapAxis = SmallestOverlapAxis;
    if (Out_SmallestOverlap) *Out_SmallestOverlap = SmallestOverlap;
    if (Out_OverlapAxisIsTriNormal) *Out_OverlapAxisIsTriNormal = OverlapAxisIsTriNormal;
    
    return false;
}

inline b32
IsSeparatingAxisTriBox(vec3 TestAxis, vec3 A, vec3 B, vec3 C, vec3 BoxExtents, vec3 *BoxAxes)
{
    f32 AProj = VecDot(A, TestAxis);
    f32 BProj = VecDot(B, TestAxis);
    f32 CProj = VecDot(C, TestAxis);
    f32 BoxRProj = (BoxExtents.E[0] * AbsF(VecDot(BoxAxes[0], TestAxis)) +
                    BoxExtents.E[1] * AbsF(VecDot(BoxAxes[1], TestAxis)) +
                    BoxExtents.E[2] * AbsF(VecDot(BoxAxes[2], TestAxis)));
    f32 MaxTriVertProj = Max(-Max(Max(AProj, BProj), CProj), Min(Min(AProj, BProj), CProj));
    b32 Result = (MaxTriVertProj > BoxRProj);
    return Result;
}

inline b32
DoesBoxIntersectPlane(vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, vec3 PlaneNormal, f32 PlaneDistance, f32 *Out_Overlap)
{
    f32 BoxRProjTowardsPlane = (BoxExtents.E[0] * AbsF(VecDot(BoxAxes[0], PlaneNormal)) +
                                BoxExtents.E[1] * AbsF(VecDot(BoxAxes[1], PlaneNormal)) +
                                BoxExtents.E[2] * AbsF(VecDot(BoxAxes[2], PlaneNormal)));

    f32 PlaneProjFromBoxC = VecDot(PlaneNormal, BoxCenter) - PlaneDistance;

    if (AbsF(PlaneProjFromBoxC) <= BoxRProjTowardsPlane)
    {
        if (Out_Overlap) *Out_Overlap = BoxRProjTowardsPlane - PlaneProjFromBoxC;
        return true;
    }
    
    return false;
}

b32
IsThereASeparatingAxisTriBoxFast(vec3 A, vec3 B, vec3 C, vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, f32 *Out_TriNormalOverlap)
{
    // NOTE: Translate triangle as conceptually moving AABB to origin
    A -= BoxCenter;
    B -= BoxCenter;
    C -= BoxCenter;
    // NOTE: Compute edge vectors of triangle
    vec3 AB = B - A;
    vec3 BC = C - B;
    vec3 CA = A - C;
    //
    // NOTE: Test exes a00..a22 (category 3)
    //
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
            if (!IsZeroVector(TestAxis) && IsSeparatingAxisTriBox(TestAxis, A, B, C, BoxExtents, BoxAxes))
            {
                return true;
            }
        }
    }
    //
    // NOTE: Test box's 3 normals (category 1)
    //
    for (u32 BoxAxisIndex = 0;
         BoxAxisIndex < 3;
         ++BoxAxisIndex)
    {
        vec3 TestAxis = BoxAxes[BoxAxisIndex];
        if (IsSeparatingAxisTriBox(TestAxis, A, B, C, BoxExtents, BoxAxes))
        {
            return true;
        }
    }
    //
    // NOTE: Test triangle's normal (category 2)
    //
    vec3 TriNormal = VecNormalize(VecCross(AB, BC));
    if (!IsZeroVector(TriNormal))
    {
        f32 TriPlaneDistance = VecDot(TriNormal, A);
        if (!DoesBoxIntersectPlane(Vec3(), BoxExtents, BoxAxes, TriNormal, TriPlaneDistance, Out_TriNormalOverlap))
        {
            return true;
        }
    }
    
    return false;
}

b32
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

b32
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
