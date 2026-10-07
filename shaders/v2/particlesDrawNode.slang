import banks;
#include "banks_shared.h"

struct VSInput {
  float3 inPosition;
  float2 uv;
  float3 normal;
  uint instanceId : SV_InstanceID;
};

struct VSOutput {
  float4 pos : SV_Position;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
  uint modelIndex = PushConstantsBank::getInt(SG_PUSH_MODEL_INDEX);

  float4x4 model = UniformBank::getFloat4x4(modelIndex);
  float4x4 view = UniformBank::getFloat4x4(SG_VIEW_INDEX);
  float4x4 proj = UniformBank::getFloat4x4(SG_PROJ_INDEX);

  float3 particlePos = particles[input.instanceId].xyz;

  float4 worldPosition = mul(model, float4(input.inPosition, 1.0f));
  worldPosition = worldPosition + float4(particlePos, 0.0f);

  VSOutput out;
  out.pos = mul(proj, mul(view, worldPosition));

  return out;
}

[shader("fragment")]
float4 fragMain(VSOutput input) { return float4(1.0f, 0.0f, 0.0f, 1.0f); }
