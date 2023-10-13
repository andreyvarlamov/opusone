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
