#pragma once

#include "../engine/vulkanRenderer.h"
#include "../utils/time.h"

#include <cstdint>
#include <vector>

struct AnimationDataFromObject
{
    uint32_t previousPositionOffset = 0;
    uint32_t nextPositionOffset = 0;
    float interpolation = 0.0f;
};

void updateAnimations();

AnimationDataFromObject getAnimationDataFromObject(const Object3D &object);