#include "particleLifeSystem.h"
#include "../utils/time.h"
#include "resourceManagementSystem.h"
#include <algorithm>
#include <vector>

static std::vector<ParticleEmitterDestroy> particleEmittersToBeDestroyed;

void addParticleEmitterToBeDestroyed(int emitterEntity, float stopDelay) {
  ParticleEmitterCpuComponent &emitter =
      getParticleEmitterCpuComponent(emitterEntity);

  ParticleEmitterDestroy toBeDestroyed{
      .emitterEntity = emitterEntity,
      .stopSpawningAfter = stopDelay,
      .destroyAfter = stopDelay + emitter.particleLifetime * 2.0f,
  };

  if (stopDelay <= 0.0f) {
    emitter.active = false;
    toBeDestroyed.stopped = true;
  }

  particleEmittersToBeDestroyed.push_back(toBeDestroyed);
}

void updateParticleEmittersToBeDestroyed() {
  for (int i = 0; i < particleEmittersToBeDestroyed.size();) {
    ParticleEmitterDestroy &toBeDestroyed = particleEmittersToBeDestroyed[i];

    toBeDestroyed.time += timeState.deltaTime;

    if (ParticleEmitterCpuComponent *emitter =
            tryGetParticleEmitterCpuComponent(toBeDestroyed.emitterEntity)) {
      const float progress =
          std::clamp(toBeDestroyed.time /
                         std::max(toBeDestroyed.destroyAfter, 0.0001f),
                     0.0f, 1.0f);

      if (emitter->lightTargetIntensity >= 0.0f) {
        if (PointLightComponent *light =
                tryGetPointLight(emitter->lightEntity)) {
          light->intensity =
              emitter->lightStartIntensity +
              (emitter->lightTargetIntensity -
               emitter->lightStartIntensity) *
                  progress;
        }
      }

      if (emitter->secondaryLightTargetIntensity >= 0.0f) {
        if (PointLightComponent *light =
                tryGetPointLight(emitter->secondaryLightEntity)) {
          light->intensity =
              emitter->secondaryLightStartIntensity +
              (emitter->secondaryLightTargetIntensity -
               emitter->secondaryLightStartIntensity) *
                  progress;
        }
      }
    }

    if (!toBeDestroyed.stopped &&
        toBeDestroyed.time >= toBeDestroyed.stopSpawningAfter) {
      if (ParticleEmitterCpuComponent *emitter =
              tryGetParticleEmitterCpuComponent(toBeDestroyed.emitterEntity)) {
        emitter->active = false;
      }

      toBeDestroyed.stopped = true;
    }

    if (toBeDestroyed.time >= toBeDestroyed.destroyAfter) {
      destroyEntity(toBeDestroyed.emitterEntity);
      if (ParticleEmitterCpuComponent *emitter =
              tryGetParticleEmitterCpuComponent(toBeDestroyed.emitterEntity)) {
        if (emitter->lightEntity != -1) {
          destroyEntity(emitter->lightEntity);
        }
        if (emitter->secondaryLightEntity != -1) {
          destroyEntity(emitter->secondaryLightEntity);
        }
      }

      particleEmittersToBeDestroyed[i] =
          std::move(particleEmittersToBeDestroyed.back());

      particleEmittersToBeDestroyed.pop_back();

      continue;
    }

    i++;
  }
}
