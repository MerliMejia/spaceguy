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

struct GlobalUniforms
{
    float4 data[TOTAL_SLOTS];
};

[[vk::binding(0,0)]]
ConstantBuffer<GlobalUniforms> globalUniforms;

float4x4 getGlobalMat4(uint index)
{
    uint start = MAT4_FROM + (index * 4);

    return float4x4(
        globalUniforms.data[start + 0],
        globalUniforms.data[start + 1],
        globalUniforms.data[start + 2],
        globalUniforms.data[start + 3]
    );
}

// Shared matrices occupy the final two logical mat4 entries.
static const uint projIndex = MAT4_COUNT - 2;
static const uint viewIndex = MAT4_COUNT - 1;
