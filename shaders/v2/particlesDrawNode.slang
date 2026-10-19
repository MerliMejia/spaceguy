import banks;
import types;
#include "banks_shared.h"

struct VSInput {
  float3 inPosition;
  float2 uv;
  float3 normal;
  uint instanceId : SV_InstanceID;
};

struct VSOutput {
  float4 pos : SV_Position;
  float2 uv;
  float particleLifeTime;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
  uint modelIndex = PushConstantsBank::getInt(SG_PUSH_MODEL_INDEX);
  int particleEmitterIndex = PushConstantsBank::getInt(SG_PUSH_PARTICLE_EMITTER_INDEX);

  float4x4 model = UniformBank::getFloat4x4(modelIndex);
  float4x4 view = UniformBank::getFloat4x4(SG_VIEW_INDEX);
  float4x4 proj = UniformBank::getFloat4x4(SG_PROJ_INDEX);
  Particle currentParticle = particles[particleEmitterIndex + input.instanceId];

  // Emitter scale = length of the model matrix's first column.
  // The bank stores matrices as rows, so the column is (m[0].x, m[1].x,
  // m[2].x).
  float size = length(float3(model[0].x, model[1].x, model[2].x));

  // Particles are now relative to the emitter pos (local pos).
  float4 worldPos = mul(model, float4(currentParticle.position, 1.0f));

  // Particle center (already world space) moved into view space.
  float4 viewPosition = mul(view, worldPos);

  // In view space +X is screen right and +Y is screen up, so offsetting
  // here keeps the quad facing the camera.
  viewPosition.xy += input.inPosition.xy * size;

  VSOutput out;
  out.pos = mul(proj, viewPosition);
  out.uv = input.uv;
  out.particleLifeTime = currentParticle.lifeTime;

  return out;
}

[shader("fragment")]
float4 fragMain(VSOutput input) {

  if (input.particleLifeTime <= 0) {
    discard;
  }

  // uv runs 0..1 across the quad, so (0.5, 0.5) is the center and
  // 0.5 is the radius of the largest circle that fits inside it.
  float distanceFromCenter = distance(input.uv, float2(0.5f, 0.5f));

  if (distanceFromCenter > 0.5f) {
    discard;
  }

  return float4(1.0f, 0.0f, 0.0f, 1.0f);
}
