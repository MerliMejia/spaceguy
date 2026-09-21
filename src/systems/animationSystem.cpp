#include "animationSystem.h"
#include "../utils/math.h"
#include "../utils/time.h"
#include "resourceManagementSystem.h"

#include <cstdint>
#include <glm/gtc/quaternion.hpp>

std::vector<Blender::V2::BlenderModel> animatedModels;
static uint32_t globalPositionOffset = 0;

static float getCurrentBlenderFrame(const AnimationComponent &animation,
                                    const Renderable &renderable,
                                    const AnimationClipGpu &clip) {

  return static_cast<float>(clip.startFrame) +
         animation.animationTimeSeconds * renderable.animatedMeshV2->fps;
}

static void updateAnimation(int entity) {
  Renderable &renderable = getRenderable(entity);
  AnimationComponent &animation = getAnimation(entity);

  if (renderable.renderKind != ObjectRenderKind::Animated ||
      renderable.animatedMeshV2 == nullptr ||
      renderable.animatedMeshV2->animations.empty()) {
    return;
  }

  if (animation.activeAnimation >=
      renderable.animatedMeshV2->animations.size()) {
    animation.activeAnimation = WizardAnimationMapping::Iddle;
    animation.animationTimeSeconds = 0.0f;
  }

  const AnimationClipGpu &clip =
      renderable.animatedMeshV2->animations[animation.activeAnimation];

  const float durationFrames =
      static_cast<float>(clip.endFrame - clip.startFrame);

  if (durationFrames <= 0.0f) {
    animation.activeFrame = static_cast<uint32_t>(clip.startFrame);
    return;
  }

  const float durationSeconds = durationFrames / renderable.animatedMeshV2->fps;

  animation.animationTimeSeconds +=
      timeState.deltaTime * animation.animationPlaySpeed;

  if (clip.loop) {
    while (animation.animationTimeSeconds >= durationSeconds) {
      animation.animationTimeSeconds -= durationSeconds;
    }

    while (animation.animationTimeSeconds < 0.0f) {
      animation.animationTimeSeconds += durationSeconds;
    }
  } else {
    if (animation.animationTimeSeconds >= durationSeconds) {
      animation.animationTimeSeconds = durationSeconds;
    }

    if (animation.animationTimeSeconds < 0.0f) {
      animation.animationTimeSeconds = 0.0f;
    }
  }

  const float currentFrame =
      getCurrentBlenderFrame(animation, renderable, clip);
  animation.activeFrame = static_cast<uint32_t>(currentFrame);
}

static TransformAnimationDataFromObject
getTransformAnimationDataFromEntity(int entity, const Renderable &renderable) {
  const AnimationComponent &animation = getAnimation(entity);

  if (renderable.renderKind != ObjectRenderKind::TransformAnimated ||
      renderable.transformAnimatedMesh == nullptr ||
      renderable.transformAnimatedMesh->animations.empty()) {
    return {};
  }

  const AnimationClipGpu &clip =
      renderable.transformAnimatedMesh->animations[animation.activeAnimation];

  const uint32_t first = clip.firstKeyPose;
  const uint32_t count = clip.keyPoseCount;

  if (count == 0) {
    return {};
  }

  const float currentFrame =
      static_cast<float>(clip.startFrame) +
      animation.animationTimeSeconds * renderable.transformAnimatedMesh->fps;

  uint32_t previousIndex = 0;
  uint32_t nextIndex = count - 1;

  for (uint32_t i = 0; i < count; ++i) {
    const TransformAnimationKeyPoseGPU &pose =
        renderable.transformAnimatedMesh->keyPoses[first + i];

    if (static_cast<float>(pose.blenderFrame) <= currentFrame) {
      previousIndex = i;
    }

    if (static_cast<float>(pose.blenderFrame) >= currentFrame) {
      nextIndex = i;
      break;
    }
  }

  const TransformAnimationKeyPoseGPU &previous =
      renderable.transformAnimatedMesh->keyPoses[first + previousIndex];

  const TransformAnimationKeyPoseGPU &next =
      renderable.transformAnimatedMesh->keyPoses[first + nextIndex];

  float interpolation = 0.0f;

  if (next.blenderFrame != previous.blenderFrame) {
    interpolation =
        (currentFrame - static_cast<float>(previous.blenderFrame)) /
        static_cast<float>(next.blenderFrame - previous.blenderFrame);
  }

  return TransformAnimationDataFromObject{
      .location = glm::mix(previous.location, next.location, interpolation),
      .rotation = glm::slerp(previous.rotation, next.rotation, interpolation),
      .scale = glm::mix(previous.scale, next.scale, interpolation),
  };
}

static float
getCurrentTransformBlenderFrame(const AnimationComponent &animation,
                                const Renderable &renderable,
                                const AnimationClipGpu &clip) {
  return static_cast<float>(clip.startFrame) +
         animation.animationTimeSeconds * renderable.transformAnimatedMesh->fps;
}

static void updateTransformAnimation(int entity) {
  Renderable &renderable = getRenderable(entity);
  AnimationComponent &animation = getAnimation(entity);
  TransformComponent &transformComponent = getTransform(entity);

  if (renderable.renderKind != ObjectRenderKind::TransformAnimated ||
      renderable.transformAnimatedMesh == nullptr ||
      renderable.transformAnimatedMesh->animations.empty()) {
    return;
  }

  if (animation.activeAnimation >=
      renderable.transformAnimatedMesh->animations.size()) {
    animation.activeAnimation = 0;
    animation.animationTimeSeconds = 0.0f;
  }

  const AnimationClipGpu &clip =
      renderable.transformAnimatedMesh->animations[animation.activeAnimation];

  const float durationFrames =
      static_cast<float>(clip.endFrame - clip.startFrame);

  if (durationFrames <= 0.0f) {
    animation.activeFrame = static_cast<uint32_t>(clip.startFrame);
    return;
  }

  const float durationSeconds =
      durationFrames / renderable.transformAnimatedMesh->fps;

  animation.animationTimeSeconds +=
      timeState.deltaTime * animation.animationPlaySpeed;

  if (clip.loop) {
    while (animation.animationTimeSeconds >= durationSeconds) {
      animation.animationTimeSeconds -= durationSeconds;
    }

    while (animation.animationTimeSeconds < 0.0f) {
      animation.animationTimeSeconds += durationSeconds;
    }
  } else {
    if (animation.animationTimeSeconds >= durationSeconds) {
      animation.animationTimeSeconds = durationSeconds;
    }

    if (animation.animationTimeSeconds < 0.0f) {
      animation.animationTimeSeconds = 0.0f;
    }
  }

  const float currentFrame =
      getCurrentTransformBlenderFrame(animation, renderable, clip);
  animation.activeFrame = static_cast<uint32_t>(currentFrame);

  TransformAnimationDataFromObject animationData =
      getTransformAnimationDataFromEntity(entity, renderable);

  glm::mat4 transform{1.0f};
  transform = glm::translate(transform, animationData.location);

  transform *= glm::mat4_cast(animationData.rotation);
  transform = glm::scale(transform, animationData.scale);

  transformComponent.model = transformComponent.baseModel * transform;
}

void updateAttachmentAnimations() {
  for (AttachmentAnimationComponent &attachment :
       resources.attachmentAnimationComponents) {
    if (!isEntityAlive(attachment.entity) ||
        !isEntityAlive(attachment.parentEntity)) {
      continue;
    }

    Renderable &parentRenderable = getRenderable(attachment.parentEntity);
    AnimationComponent &parentAnimation = getAnimation(attachment.parentEntity);

    if (parentRenderable.animatedMeshV2 == nullptr) {
      continue;
    }

    const Renderer::Types::AnimatedMesh &mesh =
        *parentRenderable.animatedMeshV2;

    if (parentAnimation.activeAnimation >= mesh.animations.size()) {
      continue;
    }

    const AnimationClipGpu &clip =
        mesh.animations[parentAnimation.activeAnimation];

    if (attachment.attachmentIndex < 0 ||
        static_cast<size_t>(attachment.attachmentIndex) >=
            clip.attachments.size()) {
      continue;
    }

    const auto &keyPoses =
        clip.attachments[attachment.attachmentIndex].keyPoses;

    if (keyPoses.empty()) {
      continue;
    }

    const float currentFrame = static_cast<float>(clip.startFrame) +
                               parentAnimation.animationTimeSeconds * mesh.fps;

    size_t previousIndex = 0;
    size_t nextIndex = keyPoses.size() - 1;

    for (size_t i = 0; i < keyPoses.size(); ++i) {
      if (static_cast<float>(keyPoses[i].blenderFrame) <= currentFrame) {
        previousIndex = i;
      }

      if (static_cast<float>(keyPoses[i].blenderFrame) >= currentFrame) {
        nextIndex = i;
        break;
      }
    }

    const auto &previous = keyPoses[previousIndex];
    const auto &next = keyPoses[nextIndex];

    float interpolation = 0.0f;

    if (next.blenderFrame != previous.blenderFrame) {
      interpolation =
          (currentFrame - static_cast<float>(previous.blenderFrame)) /
          static_cast<float>(next.blenderFrame - previous.blenderFrame);

      interpolation = glm::clamp(interpolation, 0.0f, 1.0f);
    }

    const glm::vec3 location =
        glm::mix(previous.location, next.location, interpolation);

    const glm::quat rotation = glm::normalize(
        glm::slerp(previous.rotation, next.rotation, interpolation));

    const glm::vec3 scale = glm::mix(previous.scale, next.scale, interpolation);

    TransformComponent &attachmentTransform = getTransform(attachment.entity);

    const TransformComponent &parentTransform =
        getTransform(attachment.parentEntity);

    const glm::mat4 localTransform =
        transformToModel(location, rotation, scale);

    attachmentTransform.model = parentTransform.model * localTransform;
  }
}

Blender::V2::BlenderModel loadAnimatedModel(std::string path) {
  auto newModel = Blender::V2::loadModel(path);

  newModel.globalPositionOffset = globalPositionOffset;

  for (auto &a : newModel.animations) {
    for (auto &k : a.keyPoses) {
      globalPositionOffset += k.positions.size() * 2;
    }
  }

  animatedModels.push_back(newModel);

  return newModel;
}

void initAnimations(std::vector<glm::vec4> &vertAnimSSBankData,
                    Renderer::BufferAllocation &vertAnimSBBankAllocations,
                    Renderer::VDevice &vDevice,
                    vk::raii::CommandPool &commandPool) {
  vertAnimSSBankData.reserve(SG_STORAGE_ANIMATION_SLOTS);

  for (size_t i = 0; i < SG_STORAGE_ANIMATION_SLOTS; i++) {
    vertAnimSSBankData.emplace_back(glm::vec4(0.0f));
  }

  size_t cursor = 0;
  for (auto &animatedModel : animatedModels) {
    for (auto &ani : animatedModel.animations) {
      for (auto &kp : ani.keyPoses) {
        for (size_t i = 0; i < kp.positions.size(); ++i) {
          vertAnimSSBankData[cursor++] = glm::vec4{kp.positions[i], 1.0f};
          vertAnimSSBankData[cursor++] = glm::vec4{kp.normals[i], 0.0f};
        }
      }
    }
  }

  vertAnimSBBankAllocations = Renderer::createDeviceLocalBuffer<glm::vec4>(
      vDevice, commandPool, vertAnimSSBankData,
      vk::BufferUsageFlagBits::eStorageBuffer |
          vk::BufferUsageFlagBits::eTransferDst);

  vertAnimSSBankData.clear();
  animatedModels.clear();
}

void updateAnimations() {
  for (AnimationComponent &animation : resources.animations) {
    if (!isEntityAlive(animation.entity))
      continue;
    updateAnimation(animation.entity);
    updateAttachmentAnimations();
    updateTransformAnimation(animation.entity);
  }
}

AnimationDataFromObject getAnimationDataFromEntity(int entity) {
  const Renderable &renderable = getRenderable(entity);
  const AnimationComponent &animation = getAnimation(entity);

  if (renderable.renderKind != ObjectRenderKind::Animated ||
      renderable.animatedMeshV2 == nullptr ||
      renderable.animatedMeshV2->animations.empty()) {
    return {};
  }

  const AnimationClipGpu &clip =
      renderable.animatedMeshV2->animations[animation.activeAnimation];

  const uint32_t first = clip.firstKeyPose;
  const uint32_t count = clip.keyPoseCount;

  if (count == 0) {
    return {};
  }

  const float currentFrame =
      getCurrentBlenderFrame(animation, renderable, clip);

  uint32_t previousIndex = 0;
  uint32_t nextIndex = count - 1;

  for (uint32_t i = 0; i < count; ++i) {
    const AnimationKeyPoseGpu &pose =
        renderable.animatedMeshV2->keyPoses[first + i];

    if (static_cast<float>(pose.blenderFrame) <= currentFrame) {
      previousIndex = i;
    }

    if (static_cast<float>(pose.blenderFrame) >= currentFrame) {
      nextIndex = i;
      break;
    }
  }

  const AnimationKeyPoseGpu &previousPose =
      renderable.animatedMeshV2->keyPoses[first + previousIndex];

  const AnimationKeyPoseGpu &nextPose =
      renderable.animatedMeshV2->keyPoses[first + nextIndex];

  float interpolation = 0.0f;

  if (nextPose.blenderFrame != previousPose.blenderFrame) {
    interpolation =
        (currentFrame - static_cast<float>(previousPose.blenderFrame)) /
        static_cast<float>(nextPose.blenderFrame - previousPose.blenderFrame);
  }

  return AnimationDataFromObject{
      .previousPositionOffset = previousPose.positionOffset,
      .nextPositionOffset = nextPose.positionOffset,
      .interpolation = interpolation,
  };
}

bool hasActiveAnimationEnded(int entity) {
  const Renderable &renderable = getRenderable(entity);
  const AnimationComponent &animation = getAnimation(entity);

  if (renderable.renderKind == ObjectRenderKind::Animated) {
    if (renderable.animatedMeshV2 == nullptr ||
        renderable.animatedMeshV2->animations.empty() ||
        animation.activeAnimation >=
            renderable.animatedMeshV2->animations.size()) {
      return false;
    }

    const AnimationClipGpu &clip =
        renderable.animatedMeshV2->animations[animation.activeAnimation];

    if (clip.loop) {
      return false;
    }

    const float durationSeconds =
        static_cast<float>(clip.endFrame - clip.startFrame) /
        renderable.animatedMeshV2->fps;

    return animation.animationTimeSeconds >= durationSeconds;
  }

  if (renderable.renderKind == ObjectRenderKind::TransformAnimated) {
    if (renderable.transformAnimatedMesh == nullptr ||
        renderable.transformAnimatedMesh->animations.empty() ||
        animation.activeAnimation >=
            renderable.transformAnimatedMesh->animations.size()) {
      return false;
    }

    const AnimationClipGpu &clip =
        renderable.transformAnimatedMesh->animations[animation.activeAnimation];

    if (clip.loop) {
      return false;
    }

    const float durationSeconds =
        static_cast<float>(clip.endFrame - clip.startFrame) /
        renderable.transformAnimatedMesh->fps;

    return animation.animationTimeSeconds >= durationSeconds;
  }

  return false;
}

bool hasActiveAnimationReachedFrame(int entity, float frame) {
  const Renderable &renderable = getRenderable(entity);
  const AnimationComponent &animation = getAnimation(entity);

  if (renderable.renderKind == ObjectRenderKind::Animated) {
    if (renderable.animatedMeshV2 == nullptr ||
        renderable.animatedMeshV2->animations.empty() ||
        animation.activeAnimation >=
            renderable.animatedMeshV2->animations.size()) {
      return false;
    }

    const AnimationClipGpu &clip =
        renderable.animatedMeshV2->animations[animation.activeAnimation];

    float currentFrame = getCurrentBlenderFrame(animation, renderable, clip);
    return currentFrame >= frame;
  }

  if (renderable.renderKind == ObjectRenderKind::TransformAnimated) {
    if (renderable.transformAnimatedMesh == nullptr ||
        renderable.transformAnimatedMesh->animations.empty() ||
        animation.activeAnimation >=
            renderable.transformAnimatedMesh->animations.size()) {
      return false;
    }

    const AnimationClipGpu &clip =
        renderable.transformAnimatedMesh->animations[animation.activeAnimation];

    float currentFrame = getCurrentBlenderFrame(animation, renderable, clip);
    return currentFrame >= frame;
  }

  return false;
}
