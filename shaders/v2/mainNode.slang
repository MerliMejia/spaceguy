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
  float4 sunClipPos;
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
  float4x4 sunView = UniformBank::getFloat4x4(SG_SUN_VIEW_INDEX);
  float4x4 sunProj = UniformBank::getFloat4x4(SG_SUN_PROJ_INDEX);

  float4 worldPosition = mul(model, float4(input.inPosition, 1.0f));

  float3x3 normalMatrix = inverseTranspose3x3((float3x3)model);

  VSOutput output;
  float4 clip = mul(proj, mul(view, worldPosition));
  float4 sunClip = mul(sunProj, mul(sunView, worldPosition));
  output.pos = clip;
  output.sunClipPos = sunClip;
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
  float4x4 sunView = UniformBank::getFloat4x4(SG_SUN_VIEW_INDEX);
  float4x4 sunProj = UniformBank::getFloat4x4(SG_SUN_PROJ_INDEX);

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
  float4 clip = mul(proj, mul(view, worldPosition));
  float4 sunClip = mul(sunProj, mul(sunView, worldPosition));
  output.pos = clip;
  output.sunClipPos = sunClip;
  output.uv = input.uv;
  output.worldPosition = worldPosition.xyz;
  float3x3 normalMatrix = inverseTranspose3x3((float3x3)model);
  output.worldNormal = normalize(mul(normalMatrix, norm));
  return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target {
  // Clip space -> NDC ([-1, 1] on x and y). w is 1 for ortho
  float3 ndc = input.sunClipPos.xyz / input.sunClipPos.w;

  // NDC -> texture UV ([0, 1]). Vulkan's +y down matches texture v, so no flip.
  float2 uv = ndc.xy * 0.5 + 0.5;

  int depthTexIndex =
      PushConstantsBank::getInt(SG_PUSH_SAMPLED_DEPTH_PASS_IMAGE_INDEX);
  int diffTexIndex = PushConstantsBank::getInt(SG_PUSH_DIFF_TEX_INDEX);
  float3 sunPos = UniformBank::getFloat3(SG_SUN_POS_INDEX);
  float3 sunIntensity = UniformBank::getFloat3(SG_SUN_INTENSITY_INDEX);

  Texture2D<float4> depthTestTex = textures[depthTexIndex];

  // Depth lives in .r only.
  float depth = depthTestTex.Sample(textureSampler, uv).r;

  Texture2D<float4> diffuseTexture = textures[diffTexIndex];
  float2 sampledUv = float2(input.uv.x, 1.0f - input.uv.y);
  float4 sampledColor = diffuseTexture.Sample(textureSampler, sampledUv);

  float3 N = input.worldNormal;
  float3 L = normalize(sunPos - input.worldPosition);

  float sunFactor = 0.5f;
  bool facingSun = dot(N, L) > 0;

  float lightFactor = max(dot(N, L) * sunFactor + sunFactor, 0);
  float4 upColor = float4(0.35f, 0.4f, sunFactor, 1.0f);
  float4 downColor = float4(0.15f, 0.13f, 0.12f, 1.0f);
  float ambientFactor = N.z * sunFactor + sunFactor;
  float4 ambientColor = lerp(downColor, upColor, ambientFactor);

  if (facingSun) {
    bool inShadows = (ndc.z - 0.001f <= depth) ? false : true;

    if (inShadows) {
      lightFactor = lightFactor * 0.5f;
    }
  }

  float4 shadedColor =
      (sampledColor * ambientColor) +
      (sampledColor * float4(sunIntensity, 1.0f) * lightFactor);

  return shadedColor;
}
