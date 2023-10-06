#include "opusone_collision.h"

#include "opusone_common.h"
#include "opusone_linmath.h"

inline b32
AreEdgesEqual(vec3 Edge1A, vec3 Edge1B, vec3 Edge2A, vec3 Edge2B)
{
    b32 Result = ((AreVecEqual(Edge1A, Edge2A) && AreVecEqual(Edge1B, Edge2B)) ||
                  (AreVecEqual(Edge1A, Edge2B) && AreVecEqual(Edge1B, Edge2A)));

    return Result;
}

polyhedron *
ComputePolyhedronFromVertices(memory_arena *Arena, memory_arena *TransientArena,
                              vec3 *ImportedVertices, u32 VertexCount, i32 *ImportedIndices, u32 IndexCount)
{
    Assert(Arena);
    Assert(ImportedVertices);
    Assert(VertexCount > 0);
    Assert(ImportedIndices);
    Assert(IndexCount > 0);
    Assert(IndexCount % 3 == 0);

    MemoryArena_Freeze(TransientArena);
    
    polyhedron *Polyhedron = MemoryArena_PushStruct(Arena, polyhedron);
    *Polyhedron = {};

    Polyhedron->Vertices = MemoryArena_PushArray(Arena, VertexCount, vec3);
    u32 UniqueVertexCount = 0;

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
    
    Polyhedron->VertexCount = UniqueVertexCount;
    MemoryArena_ResizePreviousPushArray(Arena, Polyhedron->VertexCount, vec3 *);

    u32 TriangleCount = IndexCount / 3;

    struct temp_edge
    {
        vec3 *PointA;
        vec3 *PointB;

        b32 IsInnerEdge;
        b32 IsDuplicate;

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
        vec3 *A = Polyhedron->Vertices + (TriangleIndex*3+0);
        vec3 *B = Polyhedron->Vertices + (TriangleIndex*3+1);
        vec3 *C = Polyhedron->Vertices + (TriangleIndex*3+2);
        
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
            *TempEdgeHead = {};
            ++UniqueNormalCount;
        }

        temp_edge *TempEdgeNode = TempEdgeHead;
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
                if (AreEdgesEqual(*TriangleEdges[TriangleEdgeIndex].PointA, *TriangleEdges[TriangleEdgeIndex].PointB,
                                  *TempEdgeNode->PointA, *TempEdgeNode->PointB))
                {
                    TriangleEdges[TriangleEdgeIndex].IsInnerEdge = true;
                    TempEdgeNode->IsInnerEdge = true;
                    --EdgePerNormalCounts[SearchTriangleIndex];
                    break;
                }
            }

            TempEdgeNode = TempEdgeNode->Next;
        }

        for (u32 TriangleEdgeIndex = 0;
             TriangleEdgeIndex < 3;
             ++TriangleEdgeIndex)
        {
            if (!TempEdgeHead->PointA)
            {
                *TempEdgeHead = TriangleEdges[TriangleEdgeIndex];
                if (!TempEdgeHead->IsInnerEdge)
                {
                    ++EdgePerNormalCounts[SearchTriangleIndex];
                }
                
                continue;
            }
            
            temp_edge *OldHead = TempEdgeHead;
            TempEdgeHead = MemoryArena_PushStruct(TransientArena, temp_edge);
            *TempEdgeHead = TriangleEdges[TriangleEdgeIndex];
            if (!TempEdgeHead->IsInnerEdge)
            {
                ++EdgePerNormalCounts[SearchTriangleIndex];
            }

            TempEdgeHead->Next = OldHead;
        }
    }

    Polyhedron->Edges = MemoryArena_PushArray(Arena, TriangleCount * 3, edge);

    u32 UniqueEdgeCount = 0;
    for (u32 UniqueNormalIndex = 0;
         UniqueNormalIndex < UniqueNormalCount-1;
         ++UniqueNormalIndex)
    {
        temp_edge *TempEdgeNode = TempEdgeHeads + UniqueNormalIndex;
        while (TempEdgeNode)
        {
            if (!TempEdgeNode->PolyhedronEdgePtr)
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
                        if (AreEdgesEqual(*TempEdgeNode->PointA, *TempEdgeNode->PointB,
                                          *TestTempEdgeNode->PointA, *TestTempEdgeNode->PointB))
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

    Polyhedron->EdgeCount = UniqueEdgeCount;
    MemoryArena_ResizePreviousPushArray(Arena, Polyhedron->EdgeCount, edge);

    Polyhedron->FaceCount = UniqueNormalCount;
    Polyhedron->Faces = MemoryArena_PushArray(Arena, Polyhedron->FaceCount, polygon);

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
        
    return Polyhedron;
}
