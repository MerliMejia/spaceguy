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

  if (p.lifeTime > 0) {
    p.lifeTime -= deltaTime;
  }

  float dist = length(p.position);
  float heading = dot(p.position, p.dir);

  float3 step = p.dir * p.speed * deltaTime;
  float3 newPos = p.position + step;
  float newHeading = dot(newPos, p.dir);

  bool hitOuter = dist >= p.outerRadius && heading > 0;
  bool hitInner = dist <= p.innerRadius && heading < 0;
  bool passingClosest = heading < 0 && newHeading >= 0;

  if (p.waitTime > 0) {
    p.waitTime -= deltaTime;
    if (p.waitTime <= 0) {
      p.dir = -p.dir;
      // Step off the turning point right away so the next frame
      // doesn't see heading ~ 0 and trigger the wait again.
      p.position += p.dir * p.speed * deltaTime;
    }
  } else if (hitOuter || hitInner) {
    p.waitTime = p.delay;
  } else if (passingClosest) {
    // Snap to the exact closest point on the path to the origin.
    float dirLenSq = dot(p.dir, p.dir);
    if (dirLenSq > 0)
      p.position -= p.dir * (heading / dirLenSq);
    p.waitTime = p.delay;
  } else {
    p.position = newPos;
  }

  particlesPositions[i] = p;
}
