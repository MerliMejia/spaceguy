import banks;
#include "banks_shared.h"

[[vk::binding(0, 1)]]
RWStructuredBuffer<float4> particlesPositions;

uint pcgHash(uint v) {
  uint state = v * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

float hashToSignedUnit(uint h) { return (float(h) / 4294967295.0) * 2.0 - 1.0; }

[shader("compute")]
[numthreads(64, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
  uint i = id.x;
  uint particlesCount =
      PushConstantsBank::getUInt(SG_PUSH_PARTICLES_COUNT_INDEX);
  float deltaTime = UniformBank::getFloat(SG_DELTA_TIME_INDEX);

  if (i >= particlesCount)
    return; // threads 100–127 do nothing

  float4 p = particlesPositions[i]; // read this particle

  uint h1 = pcgHash(i);
  uint h2 = pcgHash(h1);
  uint h3 = pcgHash(h2);

  float3 dir = normalize(
      float3(hashToSignedUnit(h1), hashToSignedUnit(h2), hashToSignedUnit(h3)));
  float speed = 1.0;

  p.xyz += dir * speed * deltaTime;
  particlesPositions[i] = p; // write it back, w untouched
}
