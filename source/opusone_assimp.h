#ifndef OPUSONE_ASSIMP_H
#define OPUSONE_ASSIMP_H

#include "opusone_common.h"
#include "opusone_linmath.h"

enum texture_type
{
    TEXTURE_TYPE_DIFFUSE,
    TEXTURE_TYPE_SPECULAR,
    TEXTURE_TYPE_EMISSION,
    TEXTURE_TYPE_NORMAL,
    TEXTURE_TYPE_COUNT
};

struct imported_material
{
    simple_string TexturePaths[TEXTURE_TYPE_COUNT];
};

#define MAX_BONE_CHILDREN 16
struct imported_bone
{
    u32 ID;
    u32 ParentID;
    // TODO: Do I actually need to store children?
    u32 ChildrenIDs[MAX_BONE_CHILDREN];
    u32 ChildrenCount;

    // TODO: Hash table to find bones instead of this?
    simple_string BoneName;
    // TODO: Save this in decomposed state?
    mat4 TransformToParent;
    // TODO: This is in mesh space, but I apply transforms to all objects in Blender,
    // and don't take into account object transforms for now
    mat4 InverseBindTransform;
};

#define MAX_BONES_PER_VERTEX 4
struct vert_bone_ids
{
    u32 D[MAX_BONES_PER_VERTEX];
};

struct vert_bone_weights
{
    f32 D[MAX_BONES_PER_VERTEX];
};

struct imported_mesh
{
    u32 VertexCount;
    u32 IndexCount;
    u32 MaterialID;
    
    vec3 *VertexPositions;
    vec3 *VertexTangents;
    vec3 *VertexBitangents;
    vec3 *VertexNormals;
    vec4 *VertexColors;
    vec2 *VertexUVs;

    vert_bone_ids *VertexBoneIDs;
    vert_bone_weights *VertexBoneWeights;
    
    i32 *Indices;
};

struct imported_armature
{
    u32 BoneCount;
    imported_bone *Bones;
};

struct imported_animation_channel
{
    u32 BoneID;
    
    u32 PositionKeyCount;
    u32 RotationKeyCount;
    u32 ScaleKeyCount;

    vec3 *PositionKeys;
    quat *RotationKeys;
    vec3 *ScaleKeys;

    f64 *PositionKeyTimes;
    f64 *RotationKeyTimes;
    f64 *ScaleKeyTimes;

    // TODO: Process scaling in animations? As I understand it's not commonly used
    // because it makes it less efficient to recalculate normals and the whole tangent space.
    // I have to do transpose(inverse(Model * boneTransform)) for every vertex, instead of
    // calculating normalMatrix = transpose(inverse(Model)) on CPU and only once per set vertices, then doing
    // normalMatrix * mat3(boneTransform) -- mat3 removing the translation component, and only keeping rotation (already no scaling).
    // But maybe it's not a big deal, and maybe it's not true that scaling is not commonly used. Not sure where I heard that from.
};

struct imported_animation
{
    u32 ChannelCount;
    imported_animation_channel *Channels;

    f64 TicksDuration;
    f64 TicksPerSecond;
    
    simple_string AnimationName;
};

struct imported_model
{
    u32 MeshCount;
    u32 MaterialCount;
    u32 AnimationCount;
    
    imported_mesh *Meshes;
    imported_material *Materials;
    imported_armature *Armature;
    imported_animation *Animations;
};

imported_model *
Assimp_LoadModel(memory_arena *AssetArena, const char *Path);

#endif
