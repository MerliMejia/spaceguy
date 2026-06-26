#include "particleLifeSystem.h"
#include "../utils/time.h"
#include "resourceManagementSystem.h"
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

      particleEmittersToBeDestroyed[i] =
          std::move(particleEmittersToBeDestroyed.back());

      particleEmittersToBeDestroyed.pop_back();

      continue;
    }

    i++;
  }
}
