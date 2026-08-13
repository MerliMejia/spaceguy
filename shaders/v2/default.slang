// Because we're using a SINGLE vertex buffer here, we don't care about the binding
// if we use 2 vertex buffer I would have to do something like:
// [[vk::binding(0)]] [[vk::location(N)]]
// [[vk::binding(0)]] [[vk::location(N + 1)]]
// [[vk::binding(1)]] [[vk::location(N)]]
// [[vk::binding(1)]] [[vk::location(N + 1)]]

struct VSInput {
    float2 inPosition; // Slang is assigning location = 0 here
    float3 inColor;// Slang is assigning location = 1 here
};

struct VSOutput
{
    float4 pos : SV_Position;
    float3 color;
};

[shader("vertex")]
VSOutput vertMain(VSInput input) {
    VSOutput output;
    output.pos = float4(input.inPosition, 0.0, 1.0);
    output.color = input.inColor;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET {
    return float4(vertIn.color, 1.0);
}
