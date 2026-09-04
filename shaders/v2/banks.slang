#include "banks_shared.h"

namespace UniformBank {
    float getFloat(uint index)
    {
        uint slot = SG_UNIFORM_FLOAT_FROM + index / 4;
        uint component = index % 4;

        return globalUniforms.data[slot][component];
    }

    float2 getFloat2(uint index)
    {
        uint slot = SG_UNIFORM_VEC2_FROM + index / 2;
        uint pair = index % 2;

        return pair == 0
            ? globalUniforms.data[slot].xy
            : globalUniforms.data[slot].zw;
    }

    float3 getFloat3(uint index)
    {
        uint slot = SG_UNIFORM_VEC3_FROM + index;
        return globalUniforms.data[slot].xyz;
    }

    float4 getFloat4(uint index)
    {
        uint slot = SG_UNIFORM_VEC4_FROM + index;
        return globalUniforms.data[slot];
    }

    float4x4 getFloat4x4(uint index)
    {
        uint start = SG_UNIFORM_MAT4_FROM + index * 4;

        return float4x4(
            globalUniforms.data[start + 0],
            globalUniforms.data[start + 1],
            globalUniforms.data[start + 2],
            globalUniforms.data[start + 3]
        );
    }
};

namespace PushConstantsBank{
    float getFloat(uint index)
    {
        uint slot = SG_PUSH_FLOAT_FROM + index / 4;
        uint component = index % 4;

        return pushConstants.floatData[slot][component];
    }

    float2 getFloat2(uint index)
    {
        uint slot = SG_PUSH_VEC2_FROM + index / 2;
        uint pair = index % 2;

        return pair == 0
            ? pushConstants.floatData[slot].xy
            : pushConstants.floatData[slot].zw;
    }

    float3 getFloat3(uint index)
    {
        uint slot = SG_PUSH_VEC3_FROM + index;
        return pushConstants.floatData[slot].xyz;
    }

    uint getUInt(uint index)
    {
        uint slot = index / 4;
        uint component = index % 4;

        return pushConstants.integerData[slot][component];
    }

    int getInt(uint index)
    {
        return asint(getUInt(index));
    }

};

struct GlobalUniforms
{
    float4 data[SG_UNIFORM_TOTAL_SLOTS];
};

[[vk::binding(0,0)]]
ConstantBuffer<GlobalUniforms> globalUniforms;

[[vk::binding(1, 0)]]
SamplerState textureSampler;
[[vk::binding(2, 0)]]
Texture2D<float4> textures[SG_MAX_TEXTURES];

struct PushConstantData
{
    // Slots 0–3: floats, float2s, and float3s
    float4 floatData[SG_PUSH_INT_FROM];

    // Slots 4–7: 16 integer values
    uint4 integerData[SG_PUSH_INT_SLOTS];
};

[[vk::push_constant]]
PushConstantData pushConstants;
