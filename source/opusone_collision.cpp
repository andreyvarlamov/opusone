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
    
    polyhedron *Polyhedron = MemoryArena_PushStruct(Arena, polyhedron);
    *Polyhedron = {};

    Polyhedron->VertexCount = VertexCount;
    Polyhedron->Vertices = MemoryArena_PushArray(Arena, Polyhedron->VertexCount, vec3);

    for (u32 VertexIndex = 0;
         VertexIndex < VertexCount;
         ++VertexIndex)
    {
        Polyhedron->Vertices[VertexIndex] = ImportedVertices[VertexIndex];
    }

    u32 TriangleCount = IndexCount / 3;

    struct temp_edge
    {
        vec3 *PointA;
        vec3 *PointB;

        b32 IsInnerEdge;
        b32 IsDuplicate;

        temp_edge *Next;
    };

    vec3 *TriangleNormals = MemoryArena_PushArray(TransientArena, TriangleCount, vec3);
    temp_edge *TempEdges = MemoryArena_PushArray(TransientArena, TriangleCount, temp_edge);

    u32 UniqueNormalCount = 0;
    for (u32 TriangleIndex = 0;
         TriangleIndex < TriangleCount;
         ++TriangleIndex)
    {
        vec3 *A = Polyhedron->Vertices + (TriangleIndex*3+0);
        vec3 *B = Polyhedron->Vertices + (TriangleIndex*3+1);
        vec3 *C = Polyhedron->Vertices + (TriangleIndex*3+2);
        
        vec3 Normal = VecNormalize(VecCross(*B-*A, *C-*B));

        temp_edge TriangleEdges[3] = { { A, B, 0, 0, 0 }, { B, C, 0, 0, 0 }, { C, A, 0, 0, 0 } };
            
        u32 SearchTriangleIndex;
        for (SearchTriangleIndex = 0;
             SearchTriangleIndex < UniqueNormalCount;
             ++SearchTriangleIndex)
        {
            if (AreVecEqual(TriangleNormals[SearchTriangleIndex], Normal))
            {
                break;
            }
        }

        temp_edge *TempEdge = TempEdges + SearchTriangleIndex;
        if (SearchTriangleIndex == UniqueNormalCount)
        {
            *TempEdge = {};
            ++UniqueNormalCount;
        }

        temp_edge *TempEdgeCursor = TempEdge;
        while (TempEdgeCursor)
        {
            if (!TempEdgeCursor->PointA)
            {
                break;
            }
            
            for (u32 TriangleEdgeIndex = 0;
                 TriangleEdgeIndex < 3;
                 ++TriangleEdgeIndex)
            {
                if (AreEdgesEqual(*EdgeVerts[TriangleEdgeIndex].Start, *EdgeVerts[TriangleEdgeIndex].End,
                                  *TempEdge->Start, *TempEdge->End))
                {
                    TempEdgeCursor->IsInvalidated = true;
                    TriangleEdges[TriangleEdgeIndex].IsInvalidated = true;
                    break;
                }
            }

            TempEdgeCursor = TempEdgeCursor->Next;
        }

        for (u32 TriangleEdgeIndex = 0;
             TriangleEdgeIndex < 3;
             ++TriangleEdgeIndex)
        {
            if (!TempEdge->Start)
            {
                *TempEdge = TriangleEdge[TriangleEdgeIndex];
                continue;
            }
            
            temp_edge *OldFirst = TempEdge;
            TempEdge = MemoryArena_PushStruct(TransientArena, temp_edge);
            *TempEdge = TriangleEdge[TriangleEdgeIndex];
            TempEdge->Next = OldFirst;
        }
    }

    u32 UniqueEdgeCount = 0;
    for (u32 TriangleNormalIndex = 0;
         TriangleNormalIndex < UniqueNormalCount-1;
         ++TriangleNormalIndex)
    {
        temp_edge *CurrentEdge = TempEdges + TriangleNormalIndex;
        while (CurrentEdge)
        {
            if (!CurrentEdge->IsDuplicate)
            {
                ++UniqueEdgeCount;
                for (u32 NormalInnerIndex = TriangleNormalIndex+1;
                     NormalInnerIndex <  UniqueNormalCount;
                     ++NormalInnerIndex)
                {
                    temp_edge *CheckEdge = TempEdges + NormalInnerIndex;
                    while (CheckEdge)
                    {
                        if (AreEdgesEqual(*CurrentEdge->Start, *CurrentEdge->End,
                                          *CheckEdge->Start, *CheckEdge->End))
                        {
                            CheckEdge->IsDuplicate = true;
                        }
                
                        CheckEdge = CheckEdge->Next;
                    }
                }
            }

            CurrentEdge = CurrentEdge->Next;
        }
    }

    Polyhedron->EdgeCount = UniqueEdgeCount;
    Polyhedron->Edges = MemoryArena_PushArray(Arena, Polyhedron->EdgeCount, edge);

    Polyhedron->FaceCount = UniqueNormalCount;
    Polyhedron->Faces = MemoryArena_PushArray(Arena, Polyhedron->FaceCount, polygon);

    vec3 *NormalCursor = TriangleNormals;
    temp_edge *EdgeListHeadCursor = TempEdges;
    polygon *FaceCursor = Polyhedron->Faces;
    for (u32 FaceIndex = 0;
         FaceIndex < Polyhedron->FaceCount;
         ++FaceIndex, ++NormalCursor, ++EdgeListHeadCursor, ++FaceCursor)
    {
        *FaceCursor = {};
        
        temp_edge *EdgeNode = EdgeListHeadCursor;
        while (EdgeNode)
        {
            if (!EdgeNode->IsInnerEdge)
            {
                ++FaceCursor->EdgeCount;
            }
            EdgeNode = EdgeNode->Next;
        }
        FaceCursor->Edges = MemoryArena(Arena, FaceCursor->EdgeCount, edge *);

        EdgeNode = EdgeListHeadCursor;
        while (EdgeNode)
        {
            if (!EdgeNode->IsInnerEdge && !EdgeNode->IsDuplicate)
            {
                
            }
            EdgeNode = EdgeNode->Next;
        }
        
        FaceCursor->Plane.Normal = *NormalCursor;
        FaceCursor->Plane.Distance = VecDot(*NormalCursor, *EdgeListHeadCursor->PointA);
    }

        
    return Polyhedron;
}
