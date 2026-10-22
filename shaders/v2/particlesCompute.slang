import banks;
import types;
#include "banks_shared.h"

[[vk::binding(0, 1)]]
RWStructuredBuffer<Particle> particlesPositions;

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {

  uint i = id.x;
  uint particlesCount = PushConstantsBank::getUInt(SG_PUSH_PARTICLES_COUNT_INDEX);

  float deltaTime = UniformBank::getFloat(SG_DELTA_TIME_INDEX);

  if (i >= particlesCount)
    return;

  Particle p = particlesPositions[i];

  if (p.isTooFar != 0)
    return;

  p.position += p.dir * p.speed * deltaTime;

  float distSqr = dot(p.position, p.position);
  float maxDistSqr = p.maxDistance * p.maxDistance;

  p.distance = distSqr;
  p.isTooFar = distSqr > maxDistSqr ? 1 : 0;

  particlesPositions[i] = p;
}
