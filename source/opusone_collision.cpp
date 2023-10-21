#include "opusone_collision.h"

#include "opusone.h"
#include "opusone_common.h"
#include "opusone_linmath.h"
#include "opusone_debug_draw.cpp"

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
            if ((Edges[EdgeIndex].AIndex == EdgeScratch[InnerIndex].AIndex && Edges[EdgeIndex].BIndex == EdgeScratch[InnerIndex].BIndex) ||
                (Edges[EdgeIndex].AIndex == EdgeScratch[InnerIndex].BIndex && Edges[EdgeIndex].BIndex == EdgeScratch[InnerIndex].AIndex))
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
                              vec3 *ImportedVertices, u32 ImportedVertexCount, i32 *ImportedIndices, u32 ImportedIndexCount,
                              polyhedron *Out_Polyhedron)
{
    Assert(Arena);
    Assert(ImportedVertices);
    Assert(ImportedVertexCount > 0);
    Assert(ImportedIndices);
    Assert(ImportedIndexCount > 0);
    Assert(ImportedIndexCount % 3 == 0);
    Assert(Out_Polyhedron);

    MemoryArena_Freeze(TransientArena);
    
    polyhedron *Polyhedron = Out_Polyhedron;
    *Polyhedron = {};

    //
    // NOTE: 1. Deduplicate vertices and save into polyhedron vertex storage.
    //
    Polyhedron->Vertices = MemoryArena_PushArray(Arena, ImportedVertexCount, vec3);
    u32 PolyhedronVertexCount = 0;

    // NOTE: Store the indices of polyhedron vertices in the original order (and duplication) to be able to use ImportedIndices.
    u32 *PolyhedronVertexIndices = MemoryArena_PushArray(TransientArena, ImportedVertexCount, u32);

    for (u32 ImportedVertexIndex = 0;
         ImportedVertexIndex < ImportedVertexCount;
         ++ImportedVertexIndex)
    {
        vec3 *ImportedVertex = ImportedVertices + ImportedVertexIndex;

        b32 DuplicateFound = false;
        for (u32 PolyhedronVertexIndex = 0;
             PolyhedronVertexIndex < PolyhedronVertexCount;
             ++PolyhedronVertexIndex)
        {
            vec3 *PolyhedronVertex = Polyhedron->Vertices + PolyhedronVertexIndex;
            
            if (AreVecEqual(*ImportedVertex, *PolyhedronVertex))
            {
                PolyhedronVertexIndices[ImportedVertexIndex] = PolyhedronVertexIndex;
                DuplicateFound = true;
                break;
            }
        }

        if (!DuplicateFound)
        {
            Polyhedron->Vertices[PolyhedronVertexCount] = *ImportedVertex;
            PolyhedronVertexIndices[ImportedVertexIndex] = PolyhedronVertexCount;
            ++PolyhedronVertexCount;
        }
    }
    
    Assert(PolyhedronVertexCount <= ImportedVertexCount);
    Polyhedron->VertexCount = PolyhedronVertexCount;
    MemoryArena_ResizePreviousPushArray(Arena, Polyhedron->VertexCount, vec3);

    u32 TriangleCount = ImportedIndexCount / 3;

    //
    // NOTE: 2. Process each triangle to make a table of unique normals and correspondent edges.
    //
    struct temp_edge
    {
        u32 AIndex;
        u32 BIndex;

        b32 IsInnerEdge;

        u32 PolyhedronEdgeIndex;
        b32 IsDuplicate;

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
        u32 TriAImportedIndex = ImportedIndices[TriangleIndex*3+0];
        u32 TriBImportedIndex = ImportedIndices[TriangleIndex*3+1];
        u32 TriCImportedIndex = ImportedIndices[TriangleIndex*3+2];
        u32 TriAPolyhedronIndex = PolyhedronVertexIndices[TriAImportedIndex];
        u32 TriBPolyhedronIndex = PolyhedronVertexIndices[TriBImportedIndex];
        u32 TriCPolyhedronIndex = PolyhedronVertexIndices[TriCImportedIndex];
        vec3 TriA = Polyhedron->Vertices[TriAPolyhedronIndex];
        vec3 TriB = Polyhedron->Vertices[TriBPolyhedronIndex];
        vec3 TriC = Polyhedron->Vertices[TriCPolyhedronIndex];

        // NOTE: Should never have a degenerate triangle at this point (if so -- something is wrong with import).
        Assert(!AreVecEqual(TriA, TriB) && !AreVecEqual(TriB, TriC) && !AreVecEqual(TriA, TriC));
        
        vec3 Normal = VecNormalize(VecCross(TriB-TriA, TriC-TriB));

        temp_edge TriangleEdges[3] = { { TriAPolyhedronIndex, TriBPolyhedronIndex },
                                       { TriBPolyhedronIndex, TriCPolyhedronIndex },
                                       { TriCPolyhedronIndex, TriAPolyhedronIndex } };
            
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

        b32 NewEdgeList = false;
        temp_edge *TempEdgeHead = TempEdgeHeads + SearchTriangleIndex;
        if (SearchTriangleIndex == UniqueNormalCount)
        {
            // NOTE: Duplicate normal wasn't found, initialize new slot.
            *TempEdgeHead = {};
            EdgePerNormalCounts[SearchTriangleIndex] = 0;
            UniqueNormals[SearchTriangleIndex] = Normal;
            ++UniqueNormalCount;
            
            NewEdgeList = true;
        }

        temp_edge *TempEdgeNode = TempEdgeHead;
        temp_edge *TempEdgeTail = TempEdgeHead;
        if (!NewEdgeList)
        {
            while (TempEdgeNode)
            {
                for (u32 TriangleEdgeIndex = 0;
                     TriangleEdgeIndex < 3;
                     ++TriangleEdgeIndex)
                {
                    if ((TriangleEdges[TriangleEdgeIndex].AIndex == TempEdgeNode->AIndex &&
                         TriangleEdges[TriangleEdgeIndex].BIndex == TempEdgeNode->BIndex) ||
                        (TriangleEdges[TriangleEdgeIndex].AIndex == TempEdgeNode->BIndex &&
                         TriangleEdges[TriangleEdgeIndex].BIndex == TempEdgeNode->AIndex))
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
        }

        for (u32 TriangleEdgeIndex = 0;
             TriangleEdgeIndex < 3;
             ++TriangleEdgeIndex)
        {
            if (NewEdgeList)
            {
                *TempEdgeTail = TriangleEdges[TriangleEdgeIndex];
                if (!TempEdgeTail->IsInnerEdge)
                {
                    ++EdgePerNormalCounts[SearchTriangleIndex];
                }
                NewEdgeList = false;
                
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

    u32 PolyhedronEdgeCount = 0;
    for (u32 UniqueNormalIndex = 0;
         UniqueNormalIndex < UniqueNormalCount;
         ++UniqueNormalIndex)
    {
        temp_edge *TempEdgeNode = TempEdgeHeads + UniqueNormalIndex;
        while (TempEdgeNode)
        {
            if (!TempEdgeNode->IsInnerEdge && !TempEdgeNode->IsDuplicate)
            {
                edge *PolyhedronEdge = Polyhedron->Edges + PolyhedronEdgeCount;
                PolyhedronEdge->AIndex = TempEdgeNode->AIndex;
                PolyhedronEdge->BIndex = TempEdgeNode->BIndex;
                PolyhedronEdge->AB = (Polyhedron->Vertices[PolyhedronEdge->BIndex] -
                                      Polyhedron->Vertices[PolyhedronEdge->AIndex]);
                TempEdgeNode->PolyhedronEdgeIndex = PolyhedronEdgeCount;

                for (u32 NormalInnerIndex = UniqueNormalIndex+1;
                     NormalInnerIndex <  UniqueNormalCount;
                     ++NormalInnerIndex)
                {
                    temp_edge *TestTempEdgeNode = TempEdgeHeads + NormalInnerIndex;
                    while (TestTempEdgeNode)
                    {
                        if ((TempEdgeNode->AIndex == TestTempEdgeNode->AIndex && TempEdgeNode->BIndex == TestTempEdgeNode->BIndex) ||
                            (TempEdgeNode->AIndex == TestTempEdgeNode->BIndex && TempEdgeNode->BIndex == TestTempEdgeNode->AIndex))
                        {
                            TestTempEdgeNode->PolyhedronEdgeIndex = PolyhedronEdgeCount;
                            TestTempEdgeNode->IsDuplicate = true;
                        }
                        
                        TestTempEdgeNode = TestTempEdgeNode->Next;
                    }
                }

                ++PolyhedronEdgeCount;
            }

            TempEdgeNode = TempEdgeNode->Next;
        }
    }

    Assert(ValidateEdgesUnique(TransientArena, Polyhedron->Edges, PolyhedronEdgeCount));

    Polyhedron->EdgeCount = PolyhedronEdgeCount;
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
        Face->Plane.Distance = VecDot(*Normal, Polyhedron->Vertices[TempEdgeHead->AIndex]);

        Face->EdgeCount = *EdgePerNormalCount;
        Face->EdgeIndices = MemoryArena_PushArray(Arena, Face->EdgeCount, u32);
        Face->VertexIndices = MemoryArena_PushArray(Arena, Face->EdgeCount * 2, u32);
        u32 VerticesAdded = 0;

        temp_edge *TempEdgeNode = TempEdgeHead;
        u32 FaceEdgeIndex = 0;
        while (TempEdgeNode)
        {
            if (!TempEdgeNode->IsInnerEdge)
            {
                Face->EdgeIndices[FaceEdgeIndex++] = TempEdgeNode->PolyhedronEdgeIndex;

                b32 FoundA = false;
                b32 FoundB = false;
                
                for (u32 VertexIndex = 0;
                     VertexIndex < VerticesAdded;
                     ++VertexIndex)
                {
                    if (Face->VertexIndices[VertexIndex] == TempEdgeNode->AIndex)
                    {
                        FoundA = true;
                    }

                    if (Face->VertexIndices[VertexIndex] == TempEdgeNode->BIndex)
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
                    Face->VertexIndices[VerticesAdded++] = TempEdgeNode->AIndex;
                }
                
                if (!FoundB)
                {
                    Face->VertexIndices[VerticesAdded++] = TempEdgeNode->BIndex;
                }
            }
            
            TempEdgeNode = TempEdgeNode->Next;
        }

        Face->VertexCount = VerticesAdded;
        MemoryArena_ResizePreviousPushArray(Arena, Face->VertexCount, u32);
    }

    MemoryArena_Unfreeze(TransientArena);
}

polyhedron_set *
CopyPolyhedronSet(memory_arena *Arena, polyhedron_set *Original)
{
    polyhedron_set *Copy = MemoryArena_PushStruct(Arena, polyhedron_set);

    Copy->PolyhedronCount = Original->PolyhedronCount;
    Copy->Polyhedra = MemoryArena_PushArray(Arena, Copy->PolyhedronCount, polyhedron);
    
    for (u32 PolyhedronIndex = 0;
         PolyhedronIndex < Copy->PolyhedronCount;
         ++PolyhedronIndex)
    {
        polyhedron *OriginalPolyhedron = Original->Polyhedra + PolyhedronIndex;
        polyhedron *CopiedPolyhedron = Copy->Polyhedra + PolyhedronIndex;
        
        CopiedPolyhedron->VertexCount = OriginalPolyhedron->VertexCount;
        CopiedPolyhedron->Vertices = MemoryArena_PushArray(Arena, CopiedPolyhedron->VertexCount, vec3);
        for(u32 VertexIndex = 0;
            VertexIndex < CopiedPolyhedron->VertexCount;
            ++VertexIndex)
        {
            CopiedPolyhedron->Vertices[VertexIndex] = OriginalPolyhedron->Vertices[VertexIndex];
        }
        
        CopiedPolyhedron->EdgeCount = OriginalPolyhedron->EdgeCount;
        CopiedPolyhedron->Edges = MemoryArena_PushArray(Arena, CopiedPolyhedron->EdgeCount, edge);
        for(u32 EdgeIndex = 0;
            EdgeIndex < CopiedPolyhedron->EdgeCount;
            ++EdgeIndex)
        {
            CopiedPolyhedron->Edges[EdgeIndex] = OriginalPolyhedron->Edges[EdgeIndex];
        }
        
        CopiedPolyhedron->FaceCount = OriginalPolyhedron->FaceCount;
        CopiedPolyhedron->Faces = MemoryArena_PushArray(Arena, CopiedPolyhedron->FaceCount, polygon);
        for(u32 FaceIndex = 0;
            FaceIndex < CopiedPolyhedron->FaceCount;
            ++FaceIndex)
        {
            polygon *OriginalFace = OriginalPolyhedron->Faces + FaceIndex;
            polygon *CopiedFace = CopiedPolyhedron->Faces + FaceIndex;

            CopiedFace->Plane = OriginalFace->Plane;
            CopiedFace->EdgeCount = OriginalFace->EdgeCount;
            CopiedFace->EdgeIndices = MemoryArena_PushArray(Arena, CopiedFace->EdgeCount, u32);
            for (u32 EdgeIndexIndex = 0;
                 EdgeIndexIndex < CopiedFace->EdgeCount;
                 ++EdgeIndexIndex)
            {
                CopiedFace->EdgeIndices[EdgeIndexIndex] = OriginalFace->EdgeIndices[EdgeIndexIndex];
            }
            CopiedFace->VertexCount = OriginalFace->VertexCount;
            CopiedFace->VertexIndices = MemoryArena_PushArray(Arena, CopiedFace->VertexCount, u32);
            for (u32 VertexIndexIndex = 0;
                 VertexIndexIndex < CopiedFace->VertexCount;
                 ++VertexIndexIndex)
            {
                CopiedFace->VertexIndices[VertexIndexIndex] = OriginalFace->VertexIndices[VertexIndexIndex];
            }
        }
    }

    return Copy;
}

polyhedron_set *
CopyAndTransformPolyhedronSet(memory_arena *Arena, polyhedron_set *Original, vec3 Position, quat Rotation, vec3 Scale)
{
    polyhedron_set *CopiedSet = CopyPolyhedronSet(Arena, Original);

    polyhedron *Polyhedron = CopiedSet->Polyhedra;
    for (u32 PolyhedronIndex = 0;
         PolyhedronIndex < CopiedSet->PolyhedronCount;
         ++PolyhedronIndex, ++Polyhedron)
    {
        vec3 *Vertex = Polyhedron->Vertices;
        for (u32 VertexIndex = 0;
             VertexIndex < Polyhedron->VertexCount;
             ++VertexIndex, ++Vertex)
        {
            FullTransformPoint(Vertex, Position, Rotation, Scale);
        }

        edge *Edge = Polyhedron->Edges;
        for (u32 EdgeIndex = 0;
             EdgeIndex < Polyhedron->EdgeCount;
             ++EdgeIndex, ++Edge)
        {
            Edge->AB = Polyhedron->Vertices[Edge->BIndex] - Polyhedron->Vertices[Edge->AIndex];
        }

        polygon *Face = Polyhedron->Faces;
        for (u32 FaceIndex = 0;
             FaceIndex < Polyhedron->FaceCount;
             ++FaceIndex, ++Face)
        {
            TransformNormal(&Face->Plane.Normal, Rotation, Scale);
        } 
    }

    return CopiedSet;
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
AreSeparatedBoxPolyhedron(polyhedron *Polyhedron, box *Box, vec3 *Out_SmallestOverlapAxis, f32 *Out_SmallestOverlap, b32 DebugPrint)
{
    vec3 SmallestOverlapAxis = {};
    f32 SmallestOverlap = FLT_MAX;
    u32 SmallestAxisIndex = 0;
    u32 AxisIndex = 0;
    
    //
    // NOTE: 1. Check box face normals
    //

    b32 FoundSeparating = false;
    
    if (DebugPrint) printf("----------------------\n");
    if (DebugPrint) printf("Box-Polyhedron SAT log:\n");
    if (DebugPrint) printf("Box face normals:\n");
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

        b32 ReplacedSmallest = false;
        if (Overlap >= 0.0f && Overlap < SmallestOverlap)
        {
            SmallestOverlap = Overlap;
            SmallestOverlapAxis = IsBoxInFront ? TestAxis : -TestAxis;
            ReplacedSmallest = true;
            SmallestAxisIndex = AxisIndex;
        }

        if (DebugPrint) printf("%d:(%d){%0.6f,%0.6f,%0.6f}:Box[%0.6f,%0.6f];Poly[%0.6f,%0.6f];BFront=%d;Overlap=%0.6f;Smallest=%d\n", AxisIndex, BoxAxisIndex, TestAxis.X,  TestAxis.Y, TestAxis.Z, BoxMinProj, BoxMaxProj, PolyhedronMinProj, PolyhedronMaxProj, IsBoxInFront, Overlap, ReplacedSmallest);

        if (Overlap < 0.0f)
        {
            FoundSeparating = true;
        }

        ++AxisIndex;
    }

    //
    // NOTE: 2. Check polyhedron face normals
    //
    if (DebugPrint) printf("Poly face normals:\n");
    for (u32 FaceIndex = 0;
         FaceIndex < Polyhedron->FaceCount;
         ++FaceIndex)
    {
        vec3 TestAxis = Polyhedron->Faces[FaceIndex].Plane.Normal;

        f32 BoxMinProj, BoxMaxProj;
        GetBoxProjOnAxisMinMax(TestAxis, Box->Center, Box->Extents, Box->Axes, &BoxMinProj, &BoxMaxProj);
        f32 PolyhedronMinProj, PolyhedronMaxProj;
        GetPolyhedronProjOnAxisMinMax(TestAxis, Polyhedron->Vertices, Polyhedron->VertexCount, &PolyhedronMinProj, &PolyhedronMaxProj);

        b32 IsBoxInFront;
        f32 Overlap = GetRangesOverlap(BoxMinProj, BoxMaxProj, PolyhedronMinProj, PolyhedronMaxProj, &IsBoxInFront);

        b32 ReplacedSmallest = false;
        if (Overlap >= 0.0f && Overlap < SmallestOverlap)
        {
            SmallestOverlap = Overlap;
            SmallestOverlapAxis = IsBoxInFront ? TestAxis : -TestAxis;
            ReplacedSmallest = true;
            SmallestAxisIndex = AxisIndex;
        }

        if (DebugPrint) printf("%d:(%d){%0.6f,%0.6f,%0.6f}:Box[%0.6f,%0.6f];Poly[%0.6f,%0.6f];BFront=%d;Overlap=%0.6f;Smallest=%d\n", AxisIndex, FaceIndex, TestAxis.X,  TestAxis.Y, TestAxis.Z, BoxMinProj, BoxMaxProj, PolyhedronMinProj, PolyhedronMaxProj, IsBoxInFront, Overlap, ReplacedSmallest);

        if (Overlap < 0.0f)
        {
            FoundSeparating = true;
        }

        ++AxisIndex;
    }

    //
    // NOTE: 3. Check cross products of edges
    //
    if (DebugPrint) printf("Edge cross products:\n");
    for (u32 BoxAxisIndex = 0;
         BoxAxisIndex < 3;
         ++BoxAxisIndex)
    {
        for (u32 PolyhedronEdgeIndex = 0;
             PolyhedronEdgeIndex < Polyhedron->EdgeCount;
             ++PolyhedronEdgeIndex)
        {
            vec3 TestAxis = VecNormalize(VecCross(Box->Axes[BoxAxisIndex], Polyhedron->Edges[PolyhedronEdgeIndex].AB));

            if (!IsZeroVector(TestAxis))
            {
                f32 BoxMinProj, BoxMaxProj;
                GetBoxProjOnAxisMinMax(TestAxis, Box->Center, Box->Extents, Box->Axes, &BoxMinProj, &BoxMaxProj);
                f32 PolyhedronMinProj, PolyhedronMaxProj;
                GetPolyhedronProjOnAxisMinMax(TestAxis, Polyhedron->Vertices, Polyhedron->VertexCount, &PolyhedronMinProj, &PolyhedronMaxProj);

                b32 IsBoxInFront;
                f32 Overlap = GetRangesOverlap(BoxMinProj, BoxMaxProj, PolyhedronMinProj, PolyhedronMaxProj, &IsBoxInFront);

                if (Overlap < 0.0f)
                {
                    FoundSeparating = true;
                }

                b32 ReplacedSmallest = false;
                if (Overlap < SmallestOverlap)
                {
                    SmallestOverlap = Overlap;
                    SmallestOverlapAxis = IsBoxInFront ? TestAxis : -TestAxis;
                    ReplacedSmallest = true;
                    SmallestAxisIndex = AxisIndex;
                }
                
                if (DebugPrint) printf("%d:(%d,%d){%0.6f,%0.6f,%0.6f}:Box[%0.6f,%0.6f];Poly[%0.6f,%0.6f];BFront=%d;p=%0.6f;Smallest=%d\n", AxisIndex, BoxAxisIndex, PolyhedronEdgeIndex, TestAxis.X,  TestAxis.Y, TestAxis.Z, BoxMinProj, BoxMaxProj, PolyhedronMinProj, PolyhedronMaxProj, IsBoxInFront, Overlap, ReplacedSmallest);
            }
            else
            {
                if (DebugPrint) printf("%d:(%d,%d){%0.6f,%0.6f,%0.6f}:Zero Vector - won't be separating\n", AxisIndex, BoxAxisIndex, PolyhedronEdgeIndex, TestAxis.X, TestAxis.Y, TestAxis.Z);
            }

            ++AxisIndex;
        }
    }

    if (Out_SmallestOverlap) *Out_SmallestOverlap = SmallestOverlap;
    if (Out_SmallestOverlapAxis) *Out_SmallestOverlapAxis = SmallestOverlapAxis;

    if (DebugPrint) printf("Done checking axes. FoundSeparating=%d; SmallestOverlap=%0.6f; SmallestOverlapAxis={%0.6f,%0.6f,%0.6f}; SmallestAxisIndex=%d\n",
                           FoundSeparating, SmallestOverlap, SmallestOverlapAxis.X, SmallestOverlapAxis.Y, SmallestOverlapAxis.Z, SmallestAxisIndex);

    return FoundSeparating;
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

void
CheckCollisionsForEntity(game_state *GameState, entity *Entity, vec3 EntityTranslation,
                         u32 MaxClosestContactCount, collision_contact *Out_ClosestContacts, b32 DebugPrint)
{
    Assert(Out_ClosestContacts);
    Assert(MaxClosestContactCount > 0);
    
    entity_type_spec *Spec = GameState->EntityTypeSpecs + Entity->Type;
    Assert(Spec->CollisionType == COLLISION_TYPE_AABB);
    Assert(Spec->CollisionGeometry);
    
    box EntityBox = {};
    EntityBox.Center = Entity->WorldPosition.P + EntityTranslation + Spec->CollisionGeometry->AABB.Center;
    EntityBox.Extents = Spec->CollisionGeometry->AABB.Extents;
    EntityBox.Axes[0] = Vec3(1,0,0);
    EntityBox.Axes[1] = Vec3(0,1,0);
    EntityBox.Axes[2] = Vec3(0,0,1);

    InitializeContactArray(Out_ClosestContacts, MaxClosestContactCount);

    for (u32 EntityIndex = 0;
         EntityIndex < GameState->EntityCount;
         ++EntityIndex)
    {
        entity *TestEntity = GameState->Entities + EntityIndex;
        if (TestEntity != Entity)
        {
            entity_type_spec *TestSpec = GameState->EntityTypeSpecs + TestEntity->Type;

            switch (TestSpec->CollisionType)
            {
                case COLLISION_TYPE_NONE:
                {
                    continue;
                } break;

                case COLLISION_TYPE_POLYHEDRON_SET:
                {
                    polyhedron_set *PolyhedronSet = CopyAndTransformPolyhedronSet(&GameState->TransientArena,
                                                                                  &TestSpec->CollisionGeometry->PolyhedronSet,
                                                                                  TestEntity->WorldPosition.P,
                                                                                  TestEntity->WorldPosition.R,
                                                                                  TestEntity->WorldPosition.S);
                        
                    polyhedron *Polyhedron = PolyhedronSet->Polyhedra;
                    for (u32 PolyhedronIndex = 0;
                         PolyhedronIndex < PolyhedronSet->PolyhedronCount;
                         ++PolyhedronIndex, ++Polyhedron)
                    {
                        f32 ThisPenetrationDepth;
                        vec3 ThisCollisionNormal;
                        b32 AreSeparated =
                            AreSeparatedBoxPolyhedron(Polyhedron, &EntityBox, &ThisCollisionNormal, &ThisPenetrationDepth, DebugPrint && TestEntity->Type == EntityType_ObstacleCourse && PolyhedronIndex == 9);

                        if (!AreSeparated)
                        {
                            b32 ContactAdded = PopulateContactArray(Out_ClosestContacts, MaxClosestContactCount, ThisCollisionNormal, ThisPenetrationDepth, TestEntity, PolyhedronIndex);
                            if (ContactAdded)
                            {
                                edge *EdgeCursor = Polyhedron->Edges;
                                for (u32 TestPolyhedronEdgeIndex = 0;
                                     TestPolyhedronEdgeIndex < Polyhedron->EdgeCount;
                                     ++TestPolyhedronEdgeIndex, ++EdgeCursor)
                                {
                                    DD_DrawQuickVector(Polyhedron->Vertices[EdgeCursor->AIndex],
                                                       Polyhedron->Vertices[EdgeCursor->BIndex],
                                                       Vec3(1,0,1));
                                }
                
                                polygon *FaceCursor = Polyhedron->Faces;
                                for (u32 FaceIndex = 0;
                                     FaceIndex < Polyhedron->FaceCount;
                                     ++FaceIndex, ++FaceCursor)
                                {
                                    vec3 FaceCentroid = Vec3();
                                    for (u32 VertexIndex = 0;
                                         VertexIndex < FaceCursor->VertexCount;
                                         ++VertexIndex)
                                    {
                                        FaceCentroid += Polyhedron->Vertices[FaceCursor->VertexIndices[VertexIndex]];
                                    }
                                    FaceCentroid /= (f32) FaceCursor->VertexCount;
                                    DD_DrawQuickVector(FaceCentroid, FaceCentroid + 0.25f*FaceCursor->Plane.Normal, Vec3(1,1,1));
                                }
                            }
                        }
                    }
                } break;
                
                case COLLISION_TYPE_AABB:
                {
                    Assert(TestSpec->CollisionGeometry);
                    aabb *TestAABB = &TestSpec->CollisionGeometry->AABB;

                    f32 ThisPenetrationDepth = FLT_MAX;
                    vec3 ThisCollisionNormal = {};
                        
                    b32 SeparatingAxisFound = false;
                    for (u32 AxisIndex = 0;
                         AxisIndex < 3;
                         ++AxisIndex)
                    {
                        f32 EntityCenter = EntityBox.Center.E[AxisIndex];
                        f32 EntityMin = EntityCenter - EntityBox.Extents.E[AxisIndex];
                        f32 EntityMax = EntityCenter + EntityBox.Extents.E[AxisIndex];

                        f32 TestEntityCenter = TestEntity->WorldPosition.P.E[AxisIndex] + TestAABB->Center.E[AxisIndex];
                        f32 TestEntityMin = TestEntityCenter - TestAABB->Extents.E[AxisIndex];
                        f32 TestEntityMax = TestEntityCenter + TestAABB->Extents.E[AxisIndex];

                        f32 PositiveAxisPenetrationDepth = TestEntityMax - EntityMin;
                        f32 NegativeAxisPenetrationDepth = EntityMax - TestEntityMin;

                        b32 NegativeAxisIsShortest = (NegativeAxisPenetrationDepth < PositiveAxisPenetrationDepth);
                        f32 AxisPenetrationDepth = (NegativeAxisIsShortest ?
                                                    NegativeAxisPenetrationDepth : PositiveAxisPenetrationDepth);

                        if (AxisPenetrationDepth < 0.0f)
                        {
                            SeparatingAxisFound = true;
                            break;
                        }

                        if (AxisPenetrationDepth < ThisPenetrationDepth)
                        {
                            ThisPenetrationDepth = AxisPenetrationDepth;
                            ThisCollisionNormal = Vec3();
                            ThisCollisionNormal.E[AxisIndex] = NegativeAxisIsShortest ? -1.0f : 1.0f;
                        }
                    }

                    if (!SeparatingAxisFound)
                    {
                        PopulateContactArray(Out_ClosestContacts, MaxClosestContactCount, ThisCollisionNormal, ThisPenetrationDepth, TestEntity, 0);
                    }

                    // DD_DrawQuickAABox(TestEntity->WorldPosition.P + TestAABB->Center, TestAABB->Extents,
                    //                   (SeparatingAxisFound ? Vec3(0,1,0) : Vec3(1,0,0)));
                } break;

                default:
                {
                    InvalidCodePath;
                } break;
            }
        }
    }
}
