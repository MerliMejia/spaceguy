#include "banks_shared.h"

namespace UniformBank {
    static const uint FLOAT_COUNT = SG_UNIFORM_FLOAT_COUNT;
    static const uint VEC2_COUNT  = SG_UNIFORM_VEC2_COUNT;
    static const uint VEC3_COUNT  = SG_UNIFORM_VEC3_COUNT;
    static const uint MAT4_COUNT  = SG_UNIFORM_MAT4_COUNT;

    static const uint FLOAT_SLOTS = SG_UNIFORM_FLOAT_SLOTS;
    static const uint VEC2_SLOTS  = SG_UNIFORM_VEC2_SLOTS;
    static const uint VEC3_SLOTS  = SG_UNIFORM_VEC3_SLOTS;
    static const uint MAT4_SLOTS  = SG_UNIFORM_MAT4_SLOTS;

    static const uint FLOAT_FROM = SG_UNIFORM_FLOAT_FROM;
    static const uint VEC2_FROM  = SG_UNIFORM_VEC2_FROM;
    static const uint VEC3_FROM  = SG_UNIFORM_VEC3_FROM;
    static const uint MAT4_FROM  = SG_UNIFORM_MAT4_FROM;

    static const uint TOTAL_SLOTS = SG_UNIFORM_TOTAL_SLOTS;

    float getFloat(uint index)
    {
        uint slot = FLOAT_FROM + index / 4;
        uint component = index % 4;

        return globalUniforms.data[slot][component];
    }

    float2 getFloat2(uint index)
    {
        uint slot = VEC2_FROM + index / 2;
        uint pair = index % 2;

        return pair == 0
            ? globalUniforms.data[slot].xy
            : globalUniforms.data[slot].zw;
    }

    float3 getFloat3(uint index)
    {
        uint slot = VEC3_FROM + index;
        return globalUniforms.data[slot].xyz;
    }

    float4x4 getFloat4x4(uint index)
    {
        uint start = MAT4_FROM + index * 4;

        return float4x4(
            globalUniforms.data[start + 0],
            globalUniforms.data[start + 1],
            globalUniforms.data[start + 2],
            globalUniforms.data[start + 3]
        );
    }
};

namespace PushConstantsBank{
    static const uint FLOAT_COUNT = SG_PUSH_FLOAT_COUNT;
    static const uint VEC2_COUNT  = SG_PUSH_VEC2_COUNT;
    static const uint VEC3_COUNT  = SG_PUSH_VEC3_COUNT;
    static const uint INT_COUNT   = SG_PUSH_INT_COUNT;

    static const uint FLOAT_SLOTS = SG_PUSH_FLOAT_SLOTS;
    static const uint VEC2_SLOTS  = SG_PUSH_VEC2_SLOTS;
    static const uint VEC3_SLOTS  = SG_PUSH_VEC3_SLOTS;
    static const uint INT_SLOTS   = SG_PUSH_INT_SLOTS;

    static const uint FLOAT_FROM = SG_PUSH_FLOAT_FROM;
    static const uint VEC2_FROM  = SG_PUSH_VEC2_FROM;
    static const uint VEC3_FROM  = SG_PUSH_VEC3_FROM;
    static const uint INT_FROM   = SG_PUSH_INT_FROM;

    static const uint TOTAL_SLOTS = SG_PUSH_TOTAL_SLOTS;
    static const uint TOTAL_BYTES = SG_PUSH_TOTAL_BYTES;

    float getFloat(uint index)
    {
        uint slot = FLOAT_FROM + index / 4;
        uint component = index % 4;

        return pushConstants.floatData[slot][component];
    }

    float2 getFloat2(uint index)
    {
        uint slot = VEC2_FROM + index / 2;
        uint pair = index % 2;

        return pair == 0
            ? pushConstants.floatData[slot].xy
            : pushConstants.floatData[slot].zw;
    }

    float3 getFloat3(uint index)
    {
        uint slot = VEC3_FROM + index;
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
    float4 data[UniformBank::TOTAL_SLOTS];
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
    float4 floatData[PushConstantsBank::INT_FROM];

    // Slots 4–7: 16 integer values
    uint4 integerData[PushConstantsBank::INT_SLOTS];
};

[[vk::push_constant]]
PushConstantData pushConstants;

// Shared matrices occupy the final two logical mat4 entries.
static const uint projIndex = SG_PROJ_INDEX;
static const uint viewIndex = SG_VIEW_INDEX;
