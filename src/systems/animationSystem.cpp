#include "animationSystem.h"
#include "resourceManagementSystem.h"

#include <glm/gtc/quaternion.hpp>

static float getCurrentBlenderFrame(const Object3D &object,
                                    const Renderable &renderable,
                                    const AnimationClipGpu &clip) {

  return static_cast<float>(clip.startFrame) +
         object.animationTimeSeconds * renderable.animatedMesh->fps;
}

static void updateAnimation(Object3D &object) {
  Renderable &renderable = getRenderable(object.renderableEntity);

  if (renderable.renderKind != ObjectRenderKind::Animated ||
      renderable.animatedMesh == nullptr ||
      renderable.animatedMesh->animations.empty()) {
    return;
  }

  if (object.activeAnimation >= renderable.animatedMesh->animations.size()) {
    object.activeAnimation = WizardAnimationMapping::Iddle;
    object.animationTimeSeconds = 0.0f;
  }

  const AnimationClipGpu &clip =
      renderable.animatedMesh->animations[object.activeAnimation];

  const float durationFrames =
      static_cast<float>(clip.endFrame - clip.startFrame);

  if (durationFrames <= 0.0f) {
    object.activeFrame = static_cast<uint32_t>(clip.startFrame);
    return;
  }

  const float durationSeconds = durationFrames / renderable.animatedMesh->fps;

  object.animationTimeSeconds +=
      timeState.deltaTime * object.animationPlaySpeed;

  if (clip.loop) {
    while (object.animationTimeSeconds >= durationSeconds) {
      object.animationTimeSeconds -= durationSeconds;
    }

    while (object.animationTimeSeconds < 0.0f) {
      object.animationTimeSeconds += durationSeconds;
    }
  } else {
    if (object.animationTimeSeconds >= durationSeconds) {
      object.animationTimeSeconds = durationSeconds;
    }

    if (object.animationTimeSeconds < 0.0f) {
      object.animationTimeSeconds = 0.0f;
    }
  }

  const float currentFrame = getCurrentBlenderFrame(object, renderable, clip);
  object.activeFrame = static_cast<uint32_t>(currentFrame);
}

static TransformAnimationDataFromObject
getTransformAnimationDataFromObject(const Object3D &object,
                                    const Renderable &renderable) {
  if (renderable.renderKind != ObjectRenderKind::TransformAnimated ||
      renderable.transformAnimatedMesh == nullptr ||
      renderable.transformAnimatedMesh->animations.empty()) {
    return {};
  }

  const AnimationClipGpu &clip =
      renderable.transformAnimatedMesh->animations[object.activeAnimation];

  const uint32_t first = clip.firstKeyPose;
  const uint32_t count = clip.keyPoseCount;

  if (count == 0) {
    return {};
  }

  const float currentFrame =
      static_cast<float>(clip.startFrame) +
      object.animationTimeSeconds * renderable.transformAnimatedMesh->fps;

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

static float getCurrentTransformBlenderFrame(const Object3D &object,
                                             const Renderable &renderable,
                                             const AnimationClipGpu &clip) {
  return static_cast<float>(clip.startFrame) +
         object.animationTimeSeconds * renderable.transformAnimatedMesh->fps;
}

static void updateTransformAnimation(Object3D &object) {
  Renderable &renderable = getRenderable(object.renderableEntity);

  if (renderable.renderKind != ObjectRenderKind::TransformAnimated ||
      renderable.transformAnimatedMesh == nullptr ||
      renderable.transformAnimatedMesh->animations.empty()) {
    return;
  }

  if (object.activeAnimation >=
      renderable.transformAnimatedMesh->animations.size()) {
    object.activeAnimation = 0;
    object.animationTimeSeconds = 0.0f;
  }

  const AnimationClipGpu &clip =
      renderable.transformAnimatedMesh->animations[object.activeAnimation];

  const float durationFrames =
      static_cast<float>(clip.endFrame - clip.startFrame);

  if (durationFrames <= 0.0f) {
    object.activeFrame = static_cast<uint32_t>(clip.startFrame);
    return;
  }

  const float durationSeconds =
      durationFrames / renderable.transformAnimatedMesh->fps;

  object.animationTimeSeconds +=
      timeState.deltaTime * object.animationPlaySpeed;

  if (clip.loop) {
    while (object.animationTimeSeconds >= durationSeconds) {
      object.animationTimeSeconds -= durationSeconds;
    }

    while (object.animationTimeSeconds < 0.0f) {
      object.animationTimeSeconds += durationSeconds;
    }
  } else {
    if (object.animationTimeSeconds >= durationSeconds) {
      object.animationTimeSeconds = durationSeconds;
    }

    if (object.animationTimeSeconds < 0.0f) {
      object.animationTimeSeconds = 0.0f;
    }
  }

  const float currentFrame =
      getCurrentTransformBlenderFrame(object, renderable, clip);
  object.activeFrame = static_cast<uint32_t>(currentFrame);

  TransformAnimationDataFromObject animationData =
      getTransformAnimationDataFromObject(object, renderable);

  glm::mat4 transform{1.0f};
  transform = glm::translate(transform, animationData.location);

  transform *= glm::mat4_cast(animationData.rotation);
  transform = glm::scale(transform, animationData.scale);

  object.model = object.baseModel * transform;
}

void updateAnimations() {
  for (Object3D &object : vulkanRendererContext.objects) {
    if (!object.enabled)
      continue;
    updateAnimation(object);
    updateTransformAnimation(object);
  }
}

AnimationDataFromObject getAnimationDataFromObject(const Object3D &object) {
  const Renderable &renderable = getRenderable(object.renderableEntity);

  if (renderable.renderKind != ObjectRenderKind::Animated ||
      renderable.animatedMesh == nullptr ||
      renderable.animatedMesh->animations.empty()) {
    return {};
  }

  const AnimationClipGpu &clip =
      renderable.animatedMesh->animations[object.activeAnimation];

  const uint32_t first = clip.firstKeyPose;
  const uint32_t count = clip.keyPoseCount;

  if (count == 0) {
    return {};
  }

  const float currentFrame = getCurrentBlenderFrame(object, renderable, clip);

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

bool hasActiveAnimationEnded(const Object3D &object) {
  const Renderable &renderable = getRenderable(object.renderableEntity);

  if (renderable.renderKind == ObjectRenderKind::Animated) {
    if (renderable.animatedMesh == nullptr ||
        renderable.animatedMesh->animations.empty() ||
        object.activeAnimation >= renderable.animatedMesh->animations.size()) {
      return false;
    }

    const AnimationClipGpu &clip =
        renderable.animatedMesh->animations[object.activeAnimation];

    if (clip.loop) {
      return false;
    }

    const float durationSeconds =
        static_cast<float>(clip.endFrame - clip.startFrame) /
        renderable.animatedMesh->fps;

    return object.animationTimeSeconds >= durationSeconds;
  }

  if (renderable.renderKind == ObjectRenderKind::TransformAnimated) {
    if (renderable.transformAnimatedMesh == nullptr ||
        renderable.transformAnimatedMesh->animations.empty() ||
        object.activeAnimation >=
            renderable.transformAnimatedMesh->animations.size()) {
      return false;
    }

    const AnimationClipGpu &clip =
        renderable.transformAnimatedMesh->animations[object.activeAnimation];

    if (clip.loop) {
      return false;
    }

    const float durationSeconds =
        static_cast<float>(clip.endFrame - clip.startFrame) /
        renderable.transformAnimatedMesh->fps;

    return object.animationTimeSeconds >= durationSeconds;
  }

  return false;
}
