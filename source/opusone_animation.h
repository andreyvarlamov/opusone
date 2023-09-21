#ifndef OPUSONE_ANIMATION_H
#define OPUSONE_ANIMATION_H

#include "opusone_common.h"
#include "opusone_linmath.h"
#include "opusone_assimp.h"

struct animation_state
{
    imported_armature *Armature;
    imported_animation *Animation;

    f64 CurrentTicks;
};

void
ComputeTransformsForAnimation(animation_state *AnimationState, mat4 *BoneTransforms, u32 BoneTransformCount);

#endif
