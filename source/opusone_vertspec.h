#ifndef OPUSONE_VERTSPEC_H
#define OPUSONE_VERTSPEC_H

enum vert_attrib_type
{
    VERT_ATTRIB_TYPE_FLOAT,
    VERT_ATTRIB_TYPE_INT,
    VERT_ATTRIB_TYPE_COUNT
};

struct vert_attrib_spec
{
    u32 Stride;
    u8 ComponentCount;
    vert_attrib_type Type;
};

#define MAX_VERT_ATTRIBS 16
struct vert_spec
{
    vert_attrib_spec AttribSpecs[MAX_VERT_ATTRIBS];
    u8 AttribCount;
    u32 VertexSize;
};

#define SetVertSpec(Out_VertSpec, VertAttribSpecs) {                    \
    vert_attrib_spec AttribSpecs[] = VertAttribSpecs;                   \
    SetVertSpec_(Out_VertSpec, AttribSpecs, ArrayCount(AttribSpecs));   \
    }

inline void
SetVertSpec_(vert_spec *Out_VertSpec, vert_attrib_spec *AttribSpecs, u8 AttribCount)
{
    Assert(Out_VertSpec);
    Assert(AttribSpecs);
    Assert(AttribCount <= MAX_VERT_ATTRIBS);
    
    for (u8 AttribIndex = 0;
         AttribIndex < AttribCount;
         ++AttribIndex)
    {
        Out_VertSpec->AttribSpecs[AttribIndex] = AttribSpecs[AttribIndex];
        Out_VertSpec->VertexSize += AttribSpecs[AttribIndex].Stride;
    }
    Out_VertSpec->AttribCount = AttribCount;
}

#define GetAttribSpec(Type, Count, ComponentCount, GLType) vert_attrib_spec { (Count) * sizeof(Type), (ComponentCount), (GLType) }

#define VERT_ATTRIB_POSITION       GetAttribSpec(vec3, 1, 3, VERT_ATTRIB_TYPE_FLOAT)
#define VERT_ATTRIB_TANGENT        GetAttribSpec(vec3, 1, 3, VERT_ATTRIB_TYPE_FLOAT)
#define VERT_ATTRIB_BITANGENT      GetAttribSpec(vec3, 1, 3, VERT_ATTRIB_TYPE_FLOAT)
#define VERT_ATTRIB_NORMAL         GetAttribSpec(vec3, 1, 3, VERT_ATTRIB_TYPE_FLOAT)
#define VERT_ATTRIB_COLOR          GetAttribSpec(vec4, 1, 4, VERT_ATTRIB_TYPE_FLOAT)
#define VERT_ATTRIB_UV             GetAttribSpec(vec2, 1, 2, VERT_ATTRIB_TYPE_FLOAT)
#define VERT_ATTRIB_BONEIDS        GetAttribSpec(u32,  4, 4, VERT_ATTRIB_TYPE_INT)
#define VERT_ATTRIB_BONEWEIGHTS    GetAttribSpec(f32,  4, 4, VERT_ATTRIB_TYPE_FLOAT)

#define VERTS_STATIC_MESH {                     \
    VERT_ATTRIB_POSITION,                       \
    VERT_ATTRIB_TANGENT,                        \
    VERT_ATTRIB_BITANGENT,                      \
    VERT_ATTRIB_NORMAL,                         \
    VERT_ATTRIB_COLOR,                          \
    VERT_ATTRIB_UV                              \
    }

#define VERTS_SKINNED_MESH {                         \
    VERT_ATTRIB_POSITION,                            \
    VERT_ATTRIB_TANGENT,                             \
    VERT_ATTRIB_BITANGENT,                           \
    VERT_ATTRIB_NORMAL,                              \
    VERT_ATTRIB_COLOR,                               \
    VERT_ATTRIB_UV,                                  \
    VERT_ATTRIB_BONEIDS,                             \
    VERT_ATTRIB_BONEWEIGHTS                          \
    }

#endif
