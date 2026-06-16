#include "animationSystem.h"

static float getCurrentBlenderFrame(const Object3D &object,
                                    const AnimationClipGpu &clip) {

  return static_cast<float>(clip.startFrame) +
         object.animationTimeSeconds * object.animatedMesh->fps;
}

static void updateAnimation(Object3D &object) {
  if (object.renderKind != ObjectRenderKind::Animated ||
      object.animatedMesh == nullptr ||
      object.animatedMesh->animations.empty()) {
    return;
  }

  if (object.activeAnimation >= object.animatedMesh->animations.size()) {
    object.activeAnimation = WizardAnimationMapping::Iddle;
    object.animationTimeSeconds = 0.0f;
  }

  const AnimationClipGpu &clip =
      object.animatedMesh->animations[object.activeAnimation];

  const float durationFrames =
      static_cast<float>(clip.endFrame - clip.startFrame);

  if (durationFrames <= 0.0f) {
    object.activeFrame = static_cast<uint32_t>(clip.startFrame);
    return;
  }

  const float durationSeconds = durationFrames / object.animatedMesh->fps;

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

  const float currentFrame = getCurrentBlenderFrame(object, clip);
  object.activeFrame = static_cast<uint32_t>(currentFrame);
}

void updateAnimations() {
  for (Object3D &object : vulkanRendererContext.objects) {
    updateAnimation(object);
  }
}

AnimationDataFromObject getAnimationDataFromObject(const Object3D &object) {
  if (object.renderKind != ObjectRenderKind::Animated ||
      object.animatedMesh == nullptr ||
      object.animatedMesh->animations.empty()) {
    return {};
  }

  const AnimationClipGpu &clip =
      object.animatedMesh->animations[object.activeAnimation];

  const uint32_t first = clip.firstKeyPose;
  const uint32_t count = clip.keyPoseCount;

  if (count == 0) {
    return {};
  }

  const float currentFrame = getCurrentBlenderFrame(object, clip);

  uint32_t previousIndex = 0;
  uint32_t nextIndex = count - 1;

  for (uint32_t i = 0; i < count; ++i) {
    const AnimationKeyPoseGpu &pose = object.animatedMesh->keyPoses[first + i];

    if (static_cast<float>(pose.blenderFrame) <= currentFrame) {
      previousIndex = i;
    }

    if (static_cast<float>(pose.blenderFrame) >= currentFrame) {
      nextIndex = i;
      break;
    }
  }

  const AnimationKeyPoseGpu &previousPose =
      object.animatedMesh->keyPoses[first + previousIndex];

  const AnimationKeyPoseGpu &nextPose =
      object.animatedMesh->keyPoses[first + nextIndex];

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
  if (object.renderKind != ObjectRenderKind::Animated ||
      object.animatedMesh == nullptr ||
      object.animatedMesh->animations.empty() ||
      object.activeAnimation >= object.animatedMesh->animations.size()) {
    return false;
  }

  const AnimationClipGpu &clip =
      object.animatedMesh->animations[object.activeAnimation];

  if (clip.loop) {
    return false;
  }

  const float durationSeconds =
      static_cast<float>(clip.endFrame - clip.startFrame) /
      object.animatedMesh->fps;

  return object.animationTimeSeconds >= durationSeconds;
}
