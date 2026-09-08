import banks;
#include "banks_shared.h"

struct VSInput
{
    float3 inPosition;
    float2 uv;
    float3 normal;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv;
    float3 worldPosition;
    float3 worldNormal;
};

float3x3 inverseTranspose3x3(float3x3 m)
{
    float3 a = m[0];
    float3 b = m[1];
    float3 c = m[2];

    float3 r0 = cross(b, c);
    float3 r1 = cross(c, a);
    float3 r2 = cross(a, b);

    return float3x3(r0, r1, r2) / dot(a, r0);
}

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    uint modelIndex = PushConstantsBank::getInt(SG_PUSH_MODEL_INDEX);

    float4x4 model = UniformBank::getFloat4x4(modelIndex);
    float4x4 view = UniformBank::getFloat4x4(SG_VIEW_INDEX);
    float4x4 proj = UniformBank::getFloat4x4(SG_PROJ_INDEX);

    float4 worldPosition = mul(
        model,
        float4(input.inPosition, 1.0f)
    );

    float3x3 normalMatrix = inverseTranspose3x3((float3x3)model);

    VSOutput output;
    output.pos = mul(proj, mul(view, worldPosition));
    output.uv = input.uv;
    output.worldPosition = worldPosition.xyz;
    output.worldNormal = mul(normalMatrix, input.normal);
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target
{
    int diffTexIndex = PushConstantsBank::getInt(SG_PUSH_DIFF_TEX_INDEX);

    Texture2D<float4> diffuseTexture = textures[diffTexIndex];

    float2 uv = float2(input.uv.x, 1.0f - input.uv.y);
    float4 sampledColor = diffuseTexture.Sample(textureSampler, uv);

    return float4(sampledColor.rgb, 1.0);
}
