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
        
    return Polyhedron;
}
