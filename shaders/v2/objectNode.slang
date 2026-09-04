import banks;
#include "banks_shared.h"

struct VSInput
{
    float3 inPosition;
    float3 inColor;
    float3 normal;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
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
    output.color = input.inColor;
    output.worldPosition = worldPosition.xyz;
    output.worldNormal = mul(normalMatrix, input.normal);
    return output;
}

float toonDiffuse(float ndotl)
{
    float width = max(fwidth(ndotl) * 1.5f, 0.01f);

    return lerp(
        0.38f,
        1.0f,
        smoothstep(
            0.42f - width,
            0.42f + width,
            ndotl
        )
    );
}

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target
{
    uint modelIndex = PushConstantsBank::getInt(SG_PUSH_MODEL_INDEX);
    float4x4 model = UniformBank::getFloat4x4(modelIndex);

    float4x4 view = UniformBank::getFloat4x4(SG_VIEW_INDEX);
    float4x4 proj = UniformBank::getFloat4x4(SG_PROJ_INDEX);
    float4 viewPos = UniformBank::getFloat4(SG_VIEW_POS_INDEX);
    float4 sunDir = UniformBank::getFloat4(SG_SUN_DIR_INDEX);
    float4 sunIntensity = UniformBank::getFloat4(SG_SUN_INTENSITY_INDEX);

    float3 N = normalize(input.worldNormal);

    float skyFactor = clamp(
        N.z * 0.5 + 0.5,
        0.0,
        1.0
    );

    float3 groundAmbient = float3(0.10, 0.12, 0.16);
    float3 skyAmbient = float3(0.28, 0.34, 0.42);

    float3 lighting = lerp(
        groundAmbient,
        skyAmbient,
        skyFactor
    );

    if (sunIntensity.a > 0.0) {
        float3 L = normalize(
            -sunDir.xyz
        );

        float ndotl = dot(N, L);
        float diffuseBand = toonDiffuse(ndotl);

        lighting += diffuseBand *
        sunIntensity.rgb *
        sunIntensity.a;
    }

    float3 finalColor = input.color * lighting;

    return float4(finalColor, 1.0);
}
