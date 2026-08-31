import banks;

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
    uint modelIndex = PushConstantsBank::getInt(0);

    float4x4 model = UniformBank::getFloat4x4(modelIndex);
    float4x4 view = UniformBank::getFloat4x4(viewIndex);
    float4x4 proj = UniformBank::getFloat4x4(projIndex);

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

[shader("fragment")]
float4 fragMain(VSOutput input) : SV_Target
{
    float3 lightPosition = float3(1.5f, 1.5f, 2.0f);

    float3 toLight = lightPosition - input.worldPosition;
    float distanceSquared = max(dot(toLight, toLight), 0.0001f);

    float3 N = normalize(input.worldNormal);
    float3 L = toLight * rsqrt(distanceSquared);

    float diffuse = max(dot(N, L), 0.0f);
    float attenuation = 1.0f / (1.0f + 0.15f * distanceSquared);
    float lighting = 0.15f + 0.85f * diffuse * attenuation;

    return float4(input.color * lighting, 1.0f);
}
