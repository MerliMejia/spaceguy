import banks;
#include "banks_shared.h"

struct VSInput {
  float3 inPosition;
  float2 uv;
  float3 normal;
};
struct AnimatedVSInput {
  float2 uv;
};

struct VSOutput {
  float4 pos : SV_Position;
  float2 uv;
  float3 worldPosition;
  float3 worldNormal;
};

float3x3 inverseTranspose3x3(float3x3 m) {
  float3 a = m[0];
  float3 b = m[1];
  float3 c = m[2];

  float3 r0 = cross(b, c);
  float3 r1 = cross(c, a);
  float3 r2 = cross(a, b);

  return float3x3(r0, r1, r2) / dot(a, r0);
}

[shader("vertex")]
VSOutput vertMain(VSInput input) {
  uint modelIndex = PushConstantsBank::getInt(SG_PUSH_MODEL_INDEX);

  float4x4 model = UniformBank::getFloat4x4(modelIndex);
  float4x4 view = UniformBank::getFloat4x4(SG_VIEW_INDEX);
  float4x4 proj = UniformBank::getFloat4x4(SG_PROJ_INDEX);

  float4 worldPosition = mul(model, float4(input.inPosition, 1.0f));

  float3x3 normalMatrix = inverseTranspose3x3((float3x3)model);

  VSOutput output;
  output.pos = mul(proj, mul(view, worldPosition));
  output.uv = input.uv;
  output.worldPosition = worldPosition.xyz;
  output.worldNormal = normalize(mul(normalMatrix, input.normal));
  return output;
}

[shader("vertex")]
VSOutput vertAnimated(AnimatedVSInput input, uint vertexId: SV_VertexID) {
  uint modelIndex = PushConstantsBank::getInt(SG_PUSH_MODEL_INDEX);

  float4x4 model = UniformBank::getFloat4x4(modelIndex);
  float4x4 view = UniformBank::getFloat4x4(SG_VIEW_INDEX);
  float4x4 proj = UniformBank::getFloat4x4(SG_PROJ_INDEX);

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
  float3 norm = lerp(prev.normal, next.normal, interpolation);

  float4 worldPosition = mul(model, float4(pos, 1.0f));

  VSOutput output;
  output.pos = mul(proj, mul(view, worldPosition));
  output.uv = input.uv;
  output.worldPosition = worldPosition.xyz;
  float3x3 normalMatrix = inverseTranspose3x3((float3x3)model);
  output.worldNormal = normalize(mul(normalMatrix, norm));
  return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
  int diffTexIndex = PushConstantsBank::getInt(SG_PUSH_DIFF_TEX_INDEX);
  float3 sunPos = UniformBank::getFloat3(SG_SUN_POS_INDEX);
  float3 cameraPos = UniformBank::getFloat3(SG_VIEW_POS_INDEX);
  float3 sunIntensity = UniformBank::getFloat3(SG_SUN_INTENSITY_INDEX);

  float3 N = input.worldNormal;
  float3 L = normalize(sunPos - input.worldPosition);

  float lightFactor = max(dot(N, L) * 0.5 + 0.5, 0);
  float4 upColor = float4(0.35f, 0.4f, 0.5f, 1.0f);
  float4 downColor = float4(0.15f, 0.13f, 0.12f, 1.0f);
  float ambientFactor = N.z * 0.5 + 0.5;
  float4 ambientColor = lerp(downColor, upColor, ambientFactor);

  Texture2D<float4> diffuseTexture = textures[diffTexIndex];

  float2 uv = float2(input.uv.x, 1.0f - input.uv.y);
  float4 sampledColor = diffuseTexture.Sample(textureSampler, uv);

  float4 color = (sampledColor * ambientColor) +
                 (sampledColor * float4(sunIntensity, 1.0f) * lightFactor);
  return color;
}
