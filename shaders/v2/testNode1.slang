import banks;

struct VSInput {
    float2 inPosition;
    float3 inColor;
    float2 uv;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
    float2 uv;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {

    uint modelIndex = PushConstantsBank::getInt(0);

    float4x4 model = UniformBank::getFloat4x4(modelIndex);
    float4x4 view  = UniformBank::getFloat4x4(viewIndex);
    float4x4 proj  = UniformBank::getFloat4x4(projIndex);

    VSOutput output;
    output.pos = mul(proj, mul(view, mul(model, float4(input.inPosition, 0.0, 1.0))));
    output.color = input.inColor;
    output.uv = input.uv;
    return output;
}

struct FragmentInput
{
    float4 pos : SV_Position;
    float3 color;
    float2 uv;
};

[shader("fragment")]
float4 fragMain(FragmentInput input) : SV_Target
{
    uint index = PushConstantsBank::getInt(2);
    float4 color = textures[index].Sample(textureSampler, input.uv);

    return float4(input.color * color.rgb, 1.0);
}
