#pragma once

struct ParticleEmitterDestroy {
  int emitterEntity = -1;
  float stopSpawningAfter = 0.0f;
  float destroyAfter = 0.0f;
  float time = 0.0f;
  bool stopped = false;
};

void addParticleEmitterToBeDestroyed(int emitterEntity, float stopDelay = 0.0f);
void updateParticleEmittersToBeDestroyed();
