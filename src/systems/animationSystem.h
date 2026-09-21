#pragma once

#include "../engine/blender/v2/importer.h"
#include "../engine/renderer/bufferUtils.h"
#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"
#include <cstdint>
#include <string>

struct AnimationDataFromObject {
  uint32_t previousPositionOffset = 0;
  uint32_t nextPositionOffset = 0;
  float interpolation = 0.0f;
};

struct TransformAnimationDataFromObject {
  glm::vec3 location{};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};
};

extern std::vector<Blender::V2::BlenderModel> animatedModels;

Blender::V2::BlenderModel loadAnimatedModel(std::string path);
void initAnimations(std::vector<glm::vec4> &vertAnimSSBankData,
                    Renderer::BufferAllocation &vertAnimSBBankAllocations,
                    Renderer::VDevice &vDevice,
                    vk::raii::CommandPool &commandPool);
void updateAnimations();

AnimationDataFromObject getAnimationDataFromEntity(int entity);
bool hasActiveAnimationEnded(int entity);
bool hasActiveAnimationReachedFrame(int entity, float frame);
