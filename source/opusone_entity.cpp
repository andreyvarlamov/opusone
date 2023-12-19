#include "opusone_entity.h"

#include "opusone.h"
#include "opusone_assimp.h"
#include "opusone_common.h"
#include "opusone_linmath.h"
#include "opusone_render.h"
#include "opusone_collision.h"
#include "opusone_animation.h"

entity *
AddEntity(game_state *GameState,
          entity_type EntityType, vec3 Position, quat Rotation, vec3 Scale, b32 IsInvisible)
{
    Assert((GameState->EntityCount + 1) < ArrayCount(GameState->Entities));

    entity *Entity = GameState->Entities + GameState->EntityCount++;
    *Entity = {};

    // NOTE: General entity params
    Entity->Type = EntityType;
    Entity->WorldPosition = WorldPosition(Position, Rotation, Scale);
    Entity->IsInvisible = IsInvisible;

    entity_type_spec *Spec = GameState->EntityTypeSpecs + EntityType;
    imported_model *ImportedModel = Spec->ImportedModel;

    // NOTE: Animation data for animated meshes
    if (EntityType != EntityType_Player &&
        ImportedModel->Armature && ImportedModel->Animations && ImportedModel->AnimationCount > 0)
    {
        Entity->AnimationState = MemoryArena_PushStruct(&GameState->WorldArena, animation_state);
        animation_state *AnimationState = Entity->AnimationState;
        *AnimationState = {};
        
        AnimationState->Armature = ImportedModel->Armature;

        switch (EntityType)
        {
            case EntityType_Enemy:
            {
                AnimationState->Animation = ImportedModel->Animations + 2;
            } break;

            default:
            {
                AnimationState->Animation = ImportedModel->Animations;
            } break;
        }
        
        AnimationState->CurrentTicks = 0.0f;
    }

    // NOTE: Add an instance of an entity type to be rendered at the model's render unit marker
    {
        render_unit *RenderUnit = Spec->RenderUnit;

        for (u32 MeshIndex = 0;
             MeshIndex < Spec->MeshCount;
             ++MeshIndex)
        {
            render_marker *Marker = RenderUnit->Markers + Spec->BaseMeshID + MeshIndex;
            Assert(Marker->StateT == RENDER_STATE_MESH);
            render_state_mesh *Mesh = &Marker->StateD.Mesh;

            b32 SlotFound = false;
            for (u32 SlotIndex = 0;
                 SlotIndex < MAX_INSTANCES_PER_MESH;
                 ++SlotIndex)
            {
                if (Mesh->EntityInstances[SlotIndex] == 0)
                {
                    Mesh->EntityInstances[SlotIndex] = Entity;
                    SlotFound = true;
                    break;
                }
            }
            Assert(SlotFound);
        }
    }

    return Entity;
}

vec3
EntityCollideWithWorld(entity *MovingEntity, vec3 OneOverEllipsoidDim, vec3 eEntityP, vec3 eEntityDeltaP, entity *TestEntities)
{
    vec3 A = Vec3(-5,0.2f,100);
    vec3 B = Vec3(-5,15,100);
    vec3 C = Vec3(-15,0.2f,90);

    vec3 eA = VecHadamard(A, OneOverEllipsoidDim);
    vec3 eB = VecHadamard(B, OneOverEllipsoidDim);
    vec3 eC = VecHadamard(C, OneOverEllipsoidDim);

    f32 TimeOfImpact;
    vec3 CollisionPoint;
    b32 FoundCollision = EntityTriangleCollide(eEntityP, eEntityDeltaP, eA, eB, eC, &TimeOfImpact, &CollisionPoint);

    ImmText_DrawQuickString(SimpleStringF("FoundCollision: %d", FoundCollision).D);

    if (!FoundCollision)
    {
        return eEntityDeltaP;
    }
    else
    {
        return eEntityDeltaP;

        // TODONEXT: Collision response if collision found (sliding); look at collision.pdf, p47 at "// *** Collision occured ***"
        // TODONEXT: Don't forget to read the actual section :)
    }
}

vec3
EntityCollideAndSlide(entity *MovingEntity, vec3 EntityEllipsoidDim, vec3 EntityP, vec3 EntityDeltaP, entity *TestEntities)
{
    vec3 OneOverEllipsoidDim = 1.0f / EntityEllipsoidDim;

    vec3 eEntityP = VecHadamard(EntityP, OneOverEllipsoidDim);
    vec3 eEntityDeltaP = VecHadamard(EntityDeltaP, OneOverEllipsoidDim);

    vec3 eAdjustedDeltaP = EntityCollideWithWorld(MovingEntity, OneOverEllipsoidDim, eEntityP, eEntityDeltaP, TestEntities);

    vec3 AdjustedDeltaP = VecHadamard(eAdjustedDeltaP, EntityEllipsoidDim);
    return AdjustedDeltaP;
}

void
EntityIntegrateAndMove(entity *MovingEntity, vec3 EntityEllipsoidDim, vec3 EntityAcc, vec3 *EntityVel,
                       f32 AccValue, f32 DragValue, f32 DeltaTime, b32 IgnoreCollisions,
                       entity *TestEntities)
{
    // NOTE: Transform acceleration vector from entity local space to world space
    // TODO: Do this better
    EntityAcc = AccValue * RotateVecByQuatSlow(VecNormalize(EntityAcc), MovingEntity->WorldPosition.R);

    // NOTE: Drag
    EntityAcc -= DragValue * (*EntityVel);
    *EntityVel += EntityAcc * DeltaTime;

    vec3 EntityP = MovingEntity->WorldPosition.P + Vec3(0,EntityEllipsoidDim.Y,0);
    vec3 EntityDeltaP = 0.5f * EntityAcc * Square(DeltaTime) + (*EntityVel) * DeltaTime;

    if (!IgnoreCollisions && VecLengthSq(EntityDeltaP) > FLT_EPSILON)
    {
        EntityDeltaP = EntityCollideAndSlide(MovingEntity, EntityEllipsoidDim, EntityP, EntityDeltaP, TestEntities);
    }

    MovingEntity->WorldPosition.P += EntityDeltaP;
}
