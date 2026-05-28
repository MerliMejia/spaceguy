#include "vulkanDescriptorSetLayouts.h"
#include "../vulkanGlobals.h"

void DEFAULT_DESCRIPTOR_SET_LAYOUT(vk::raii::DescriptorSetLayout &descriptorSetLayout, vk::raii::Device &device)
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = 1,
        .pBindings = &uboLayoutBinding};

    descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
};
void DEFAULT_DESCRIPTOR_POOL(vk::raii::DescriptorPool &descriptorPool, vk::raii::Device &device)
{
    vk::DescriptorPoolSize poolSize{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};

    vk::DescriptorPoolCreateInfo poolInfo{
        .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize};

    descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
};