import banks;

struct VSInput {
    float2 inPosition;
    float3 inColor;
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {

    uint modelIndex = PushConstantsBank::getInt(3);

    float4x4 model = UniformBank::getFloat4x4(modelIndex);
    float4x4 view  = UniformBank::getFloat4x4(viewIndex);
    float4x4 proj  = UniformBank::getFloat4x4(projIndex);

    VSOutput output;
    output.pos = mul(proj, mul(view, mul(model, float4(input.inPosition, 0.0, 1.0))));
    output.color = input.inColor;
    return output;
}


[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(vertIn.color, 1.0);
}
