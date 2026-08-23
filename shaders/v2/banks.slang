namespace UniformBank {
    static const uint FLOAT_COUNT = 100;
    static const uint VEC2_COUNT = 100;
    static const uint VEC3_COUNT = 100;
    static const uint MAT4_COUNT = 100;

    // How many float4 slots each region needs
    static const uint FLOAT_SLOTS = (FLOAT_COUNT + 3) / 4;
    static const uint VEC2_SLOTS  = (VEC2_COUNT + 1) / 2;
    static const uint VEC3_SLOTS  = VEC3_COUNT;
    static const uint MAT4_SLOTS  = MAT4_COUNT * 4;

    // Automatically derived offsets
    static const uint FLOAT_FROM = 0;
    static const uint VEC2_FROM  = FLOAT_FROM + FLOAT_SLOTS;
    static const uint VEC3_FROM  = VEC2_FROM  + VEC2_SLOTS;
    static const uint MAT4_FROM  = VEC3_FROM  + VEC3_SLOTS;

    static const uint TOTAL_SLOTS = MAT4_FROM + MAT4_SLOTS;

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
    // 4 floats
    // 4 float2s
    // 1 float3
    // 16 int/uint values

    static const uint FLOAT_COUNT = 4;
    static const uint VEC2_COUNT  = 4;
    static const uint VEC3_COUNT  = 1;
    static const uint INT_COUNT   = 16;

    static const uint FLOAT_SLOTS = (FLOAT_COUNT + 3) / 4;
    static const uint VEC2_SLOTS  = (VEC2_COUNT + 1) / 2;
    static const uint VEC3_SLOTS  = VEC3_COUNT;
    static const uint INT_SLOTS   = (INT_COUNT + 3) / 4;

    static const uint FLOAT_FROM = 0;
    static const uint VEC2_FROM  = FLOAT_FROM + FLOAT_SLOTS;
    static const uint VEC3_FROM  = VEC2_FROM + VEC2_SLOTS;
    static const uint INT_FROM   = VEC3_FROM + VEC3_SLOTS;

    static const uint TOTAL_SLOTS = INT_FROM + INT_SLOTS;
    static const uint TOTAL_BYTES = TOTAL_SLOTS * 16;

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
static const uint projIndex = UniformBank::MAT4_COUNT - 2;
static const uint viewIndex = UniformBank::MAT4_COUNT - 1;
