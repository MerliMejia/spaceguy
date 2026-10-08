import banks;
import types;
#include "banks_shared.h"

[[vk::binding(0, 1)]]
RWStructuredBuffer<Particle> particlesPositions;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
  uint i = id.x;
  uint particlesCount =
      PushConstantsBank::getUInt(SG_PUSH_PARTICLES_COUNT_INDEX);
  float3 emitterPosition =
      PushConstantsBank::getFloat3(SG_PUSH_PARTICLES_EMITTER_POS_INDEX);

  float deltaTime = UniformBank::getFloat(SG_DELTA_TIME_INDEX);

  if (i >= particlesCount)
    return; // threads 100–127 do nothing

  Particle p = particlesPositions[i];

  if (p.lifeTime > 0) {
    p.lifeTime -= deltaTime;
  }

  if (distance(emitterPosition, p.position) > 0.1f) {
    p.position += p.dir.xyz * p.dir.w * deltaTime;
  }

  particlesPositions[i] = p;
}
