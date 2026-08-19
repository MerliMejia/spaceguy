import banks;

struct VSInput {
    float2 inPosition;
    float3 inColor;
};

struct PushConstants
{
    uint modelIndex;
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {

    float4x4 model = getGlobalMat4(pc.modelIndex);
    float4x4 view  = getGlobalMat4(viewIndex);
    float4x4 proj  = getGlobalMat4(projIndex);

    VSOutput output;
    output.pos = mul(proj, mul(view, mul(model, float4(input.inPosition, 0.0, 1.0))));
    output.color = input.inColor;
    return output;
}


[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(vertIn.color, 1.0);
}
