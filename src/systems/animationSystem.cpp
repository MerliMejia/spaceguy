#include "animationSystem.h"
#include "resourceManagementSystem.h"

#include <glm/gtc/quaternion.hpp>

static float getCurrentBlenderFrame(const AnimationComponent &animation,
                                    const Renderable &renderable,
                                    const AnimationClipGpu &clip) {

  return static_cast<float>(clip.startFrame) +
         animation.animationTimeSeconds * renderable.animatedMesh->fps;
}

static void updateAnimation(int entity) {
  Renderable &renderable = getRenderable(entity);
  AnimationComponent &animation = getAnimation(entity);

  if (renderable.renderKind != ObjectRenderKind::Animated ||
      renderable.animatedMesh == nullptr ||
      renderable.animatedMesh->animations.empty()) {
    return;
  }

  if (animation.activeAnimation >= renderable.animatedMesh->animations.size()) {
    animation.activeAnimation = WizardAnimationMapping::Iddle;
    animation.animationTimeSeconds = 0.0f;
  }

  const AnimationClipGpu &clip =
      renderable.animatedMesh->animations[animation.activeAnimation];

  const float durationFrames =
      static_cast<float>(clip.endFrame - clip.startFrame);

  if (durationFrames <= 0.0f) {
    animation.activeFrame = static_cast<uint32_t>(clip.startFrame);
    return;
  }

  const float durationSeconds = durationFrames / renderable.animatedMesh->fps;

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

  const float currentFrame = getCurrentBlenderFrame(animation, renderable, clip);
  animation.activeFrame = static_cast<uint32_t>(currentFrame);
}

static TransformAnimationDataFromObject
getTransformAnimationDataFromEntity(int entity,
                                    const Renderable &renderable) {
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

static float getCurrentTransformBlenderFrame(const AnimationComponent &animation,
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

void updateAnimations() {
  for (Renderable &renderable : resources.renderables) {
    if (!isEntityAlive(renderable.entity) || !renderable.visible)
      continue;
    if (tryGetAnimation(renderable.entity) == nullptr) {
      continue;
    }
    updateAnimation(renderable.entity);
    updateTransformAnimation(renderable.entity);
  }
}

AnimationDataFromObject getAnimationDataFromEntity(int entity) {
  const Renderable &renderable = getRenderable(entity);
  const AnimationComponent &animation = getAnimation(entity);

  if (renderable.renderKind != ObjectRenderKind::Animated ||
      renderable.animatedMesh == nullptr ||
      renderable.animatedMesh->animations.empty()) {
    return {};
  }

  const AnimationClipGpu &clip =
      renderable.animatedMesh->animations[animation.activeAnimation];

  const uint32_t first = clip.firstKeyPose;
  const uint32_t count = clip.keyPoseCount;

  if (count == 0) {
    return {};
  }

  const float currentFrame = getCurrentBlenderFrame(animation, renderable, clip);

  uint32_t previousIndex = 0;
  uint32_t nextIndex = count - 1;

  for (uint32_t i = 0; i < count; ++i) {
    const AnimationKeyPoseGpu &pose =
        renderable.animatedMesh->keyPoses[first + i];

    if (static_cast<float>(pose.blenderFrame) <= currentFrame) {
      previousIndex = i;
    }

    if (static_cast<float>(pose.blenderFrame) >= currentFrame) {
      nextIndex = i;
      break;
    }
  }

  const AnimationKeyPoseGpu &previousPose =
      renderable.animatedMesh->keyPoses[first + previousIndex];

  const AnimationKeyPoseGpu &nextPose =
      renderable.animatedMesh->keyPoses[first + nextIndex];

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
    if (renderable.animatedMesh == nullptr ||
        renderable.animatedMesh->animations.empty() ||
        animation.activeAnimation >= renderable.animatedMesh->animations.size()) {
      return false;
    }

    const AnimationClipGpu &clip =
        renderable.animatedMesh->animations[animation.activeAnimation];

    if (clip.loop) {
      return false;
    }

    const float durationSeconds =
        static_cast<float>(clip.endFrame - clip.startFrame) /
        renderable.animatedMesh->fps;

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
