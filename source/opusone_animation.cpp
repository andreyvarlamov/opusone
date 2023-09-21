#include "opusone_animation.h"

#include "opusone_common.h"
#include "opusone_linmath.h"
#include "opusone_assimp.h"

#include <cstdio>

void
ComputeTransformsForAnimation(animation_state *AnimationState, mat4 *BoneTransforms, u32 BoneTransformCount)
{
    imported_armature *Armature = AnimationState->Armature;
    imported_animation *Animation = AnimationState->Animation;
    Assert(Armature);
    Assert(Animation);
    Assert(Armature->BoneCount == BoneTransformCount);
    
    for (u32 BoneIndex = 1;
         BoneIndex < Armature->BoneCount;
         ++BoneIndex)
    {
        imported_bone *Bone = Armature->Bones + BoneIndex;

        mat4 *Transform = BoneTransforms + BoneIndex;
                            
        imported_animation_channel *Channel = Animation->Channels + BoneIndex;
        Assert(Channel->BoneID == BoneIndex);

        // NOTE: Can't just use a "no change" key, because animation is supposed
        // to include bone's transform to parent. If there are such cases, will
        // need to handle them specially
        Assert(Channel->PositionKeyCount != 0);
        Assert(Channel->RotationKeyCount != 0);
        Assert(Channel->ScaleKeyCount != 0);

        vec3 Position = {};

        if (Channel->PositionKeyCount == 1)
        {
            Position = Channel->PositionKeys[0];
        }
        else if (Channel->PositionKeyCount > 1)
        {
            u32 PrevTimeIndex = 0;
            u32 TimeIndex = 0;
            for (;
                 TimeIndex < Channel->PositionKeyCount;
                 ++TimeIndex)
            {
                if (AnimationState->CurrentTicks <= Channel->PositionKeyTimes[TimeIndex])
                {
                    break;
                }
                PrevTimeIndex = TimeIndex;
            }
            Assert(TimeIndex < Channel->PositionKeyCount);
            // NOTE: If the current animation time is at exactly 0.0, both keys will be the same,
            // lerp will have no effect. No need to branch, because that should happen only rarely anyways.
                                
            vec3 PositionA = Channel->PositionKeys[PrevTimeIndex];
            vec3 PositionB = Channel->PositionKeys[TimeIndex];

            f32 T = (f32) ((AnimationState->CurrentTicks - Channel->PositionKeyTimes[PrevTimeIndex]) /
                           (Channel->PositionKeyTimes[TimeIndex] - Channel->PositionKeyTimes[PrevTimeIndex]));

            Position = Vec3Lerp(PositionA, PositionB, T);

#if 0
            if (ShouldLog && PrevTimeIndex == 0)
            {
                printf("[%s]-P-(%d)<%0.3f,%0.3f,%0.3f>:(%d)<%0.3f,%0.3f,%0.3f>[%0.3f]<%0.3f,%0.3f,%0.3f>\n",
                       Bone->BoneName.D,
                       PrevTimeIndex, PositionA.X, PositionA.Y, PositionA.Z,
                       TimeIndex, PositionB.X, PositionB.Y, PositionB.Z,
                       T, Position.X, Position.Y, Position.Z);
            }
#endif
        }
                            
        quat Rotation = Quat();

        if (Channel->RotationKeyCount == 1)
        {
            Rotation = Channel->RotationKeys[0];
        }
        else if (Channel->PositionKeyCount > 1)
        {
            u32 PrevTimeIndex = 0;
            u32 TimeIndex = 0;
            for (;
                 TimeIndex < Channel->PositionKeyCount;
                 ++TimeIndex)
            {
                if (AnimationState->CurrentTicks <= Channel->RotationKeyTimes[TimeIndex])
                {
                    break;
                }
                PrevTimeIndex = TimeIndex;
            }
            Assert(TimeIndex < Channel->RotationKeyCount);

            quat RotationA = Channel->RotationKeys[PrevTimeIndex];
            quat RotationB = Channel->RotationKeys[TimeIndex];

            f32 T = (f32) ((AnimationState->CurrentTicks - Channel->RotationKeyTimes[PrevTimeIndex]) /
                           (Channel->RotationKeyTimes[TimeIndex] - Channel->RotationKeyTimes[PrevTimeIndex]));

            Rotation = QuatSphericalLerp(RotationA, RotationB, T);

#if 0
            if (ShouldLog && PrevTimeIndex == 0)
            {
                printf("[%s]-R-(%d)<%0.3f,%0.3f,%0.3f,%0.3f>:(%d)<%0.3f,%0.3f,%0.3f,%0.3f>[%0.3f]<%0.3f,%0.3f,%0.3f,%0.3f>\n",
                       Bone->BoneName.D,
                       PrevTimeIndex, RotationA.W, RotationA.X, RotationA.Y, RotationA.Z,
                       TimeIndex, RotationB.W, RotationB.X, RotationB.Y, RotationB.Z,
                       T, Rotation.W, Rotation.X, Rotation.Y, Rotation.Z);
            }
#endif
        }
                            
        vec3 Scale = Vec3(1.0f, 1.0f, 1.0f);

        if (Channel->ScaleKeyCount == 1)
        {
            Scale = Channel->ScaleKeys[0];
        }
        else if (Channel->ScaleKeyCount > 1)
        {
            u32 PrevTimeIndex = 0;
            u32 TimeIndex = 0;
            for (;
                 TimeIndex < Channel->ScaleKeyCount;
                 ++TimeIndex)
            {
                if (AnimationState->CurrentTicks <= Channel->ScaleKeyTimes[TimeIndex])
                {
                    break;
                }
                PrevTimeIndex = TimeIndex;
            }
            Assert(TimeIndex < Channel->RotationKeyCount);

            vec3 ScaleA = Channel->ScaleKeys[PrevTimeIndex];
            vec3 ScaleB = Channel->ScaleKeys[TimeIndex];

            f32 T = (f32) ((AnimationState->CurrentTicks - Channel->ScaleKeyTimes[PrevTimeIndex]) /
                           (Channel->ScaleKeyTimes[TimeIndex] - Channel->ScaleKeyTimes[PrevTimeIndex]));

            Scale = Vec3Lerp(ScaleA, ScaleB, T);

#if 0
            if (ShouldLog && PrevTimeIndex == 0)
            {
                printf("[%s]-S-(%d)<%0.3f,%0.3f,%0.3f>:(%d)<%0.3f,%0.3f,%0.3f>[%0.3f]<%0.3f,%0.3f,%0.3f>\n",
                       Bone->BoneName.D,
                       PrevTimeIndex, ScaleA.X, ScaleA.Y, ScaleA.Z,
                       TimeIndex, ScaleB.X, ScaleB.Y, ScaleB.Z,
                       T, Scale.X, Scale.Y, Scale.Z);
            }
#endif
        }

        mat4 AnimationTransform = Mat4GetFullTransform(Position, Rotation, Scale);

        if (Bone->ParentID == 0)
        {
            *Transform = AnimationTransform;
        }
        else
        {
            Assert(Bone->ParentID < BoneIndex);
            *Transform = BoneTransforms[Bone->ParentID] * AnimationTransform;
        }
    }
}
