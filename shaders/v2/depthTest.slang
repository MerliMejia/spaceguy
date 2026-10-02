import banks;
#include "banks_shared.h"

struct VSInput {
  float3 inPosition;
};

struct VSOutput {
  float4 pos : SV_Position;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
  uint modelIndex = PushConstantsBank::getInt(SG_PUSH_MODEL_INDEX);

  float4x4 model = UniformBank::getFloat4x4(modelIndex);
  float4x4 view = UniformBank::getFloat4x4(SG_SUN_VIEW_INDEX);
  float4x4 proj = UniformBank::getFloat4x4(SG_SUN_PROJ_INDEX);

  float4 worldPosition = mul(model, float4(input.inPosition, 1.0f));

  VSOutput output;
  output.pos = mul(proj, mul(view, worldPosition));
  return output;
}

[shader("vertex")]
VSOutput vertAnimated(uint vertexId: SV_VertexID) {
  uint modelIndex = PushConstantsBank::getInt(SG_PUSH_MODEL_INDEX);

  float4x4 model = UniformBank::getFloat4x4(modelIndex);
  float4x4 view = UniformBank::getFloat4x4(SG_SUN_VIEW_INDEX);
  float4x4 proj = UniformBank::getFloat4x4(SG_SUN_PROJ_INDEX);

  uint previousPositionOffset =
      PushConstantsBank::getUInt(SG_PUSH_PREV_POSE_INDEX);
  uint nextPositionOffset = PushConstantsBank::getUInt(SG_PUSH_NEXT_POSE_INDEX);
  float interpolation =
      PushConstantsBank::getFloat(SG_PUS_INTERPOLATION_FACTOR_INDEX);

  let prev = AnimationsStorageBufferBank::getPositionNormal(
      previousPositionOffset, vertexId);
  let next = AnimationsStorageBufferBank::getPositionNormal(nextPositionOffset,
                                                            vertexId);

  float3 pos = lerp(prev.position, next.position, interpolation);

  float4 worldPosition = mul(model, float4(pos, 1.0f));

  VSOutput output;
  output.pos = mul(proj, mul(view, worldPosition));
  return output;
}
