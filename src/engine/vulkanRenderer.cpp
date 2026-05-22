#include "vulkanRenderer.h"
#include "./predefined/vulkanDescriptorSetLayouts.h"
#include "./predefined/vulkanGraphicPipelines.h"
#include "./vulkanBackend.h"

VulkanRendererContext vulkanRendererContext{};

struct BufferWithMemory
{
    vk::raii::Buffer buffer;
    vk::raii::DeviceMemory memory;
};

void createDescriptorSetLayout()
{
    CREATE_SIMPLE_DESCRIPTOR_SET_LAYOUT(vulkanRendererContext.descriptorSetLayout, vulkanContext.device);
}

void createDescriptorPool()
{
    CREATE_SIMPLE_DESCRIPTOR_POOL(vulkanRendererContext.descriptorPool, vulkanContext.device);
}

void createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(
        MAX_FRAMES_IN_FLIGHT,
        *vulkanRendererContext.descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInf{
        .descriptorPool = *vulkanRendererContext.descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .pSetLayouts = layouts.data()};

    vulkanRendererContext.descriptorSets = vk::raii::DescriptorSets(vulkanContext.device, allocInf);

    // The way we set up descriptor sets here should change based on how we render stuff.
    // I just don't know a good way to abstract this yet.
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = *vulkanRendererContext.uniformBuffers[i],
            .offset = 0,
            .range = sizeof(CameraBufferObject)};

        vk::WriteDescriptorSet descriptorWrite{
            .dstSet = *vulkanRendererContext.descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfo,
            .pTexelBufferView = nullptr};

        vulkanContext.device.updateDescriptorSets(descriptorWrite, nullptr);
    }
}

void createGraphicsPipeline()
{
    CREATE_SIMPLE_CAMERA_MODEL_GP();
}

uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties memProperties = vulkanContext.physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        bool typeMatches = typeFilter & (1 << i);
        bool hasPropertoes =
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties;

        if (typeMatches && hasPropertoes)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory");
}

BufferWithMemory createBuffer(
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    vk::BufferCreateInfo bufferInfo{
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive};

    vk::raii::Buffer buffer = vk::raii::Buffer(vulkanContext.device, bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(
            memRequirements.memoryTypeBits,
            properties)};

    vk::raii::DeviceMemory bufferMemory(vulkanContext.device, allocInfo);

    buffer.bindMemory(*bufferMemory, 0);

    return {
        std::move(buffer),
        std::move(bufferMemory)};
}

void createUniformBuffers()
{
    vk::DeviceSize bufferSize = sizeof(CameraBufferObject);

    vulkanRendererContext.uniformBuffers.clear();
    vulkanRendererContext.uniformBuffersMemory.clear();
    vulkanRendererContext.uniformBuffersMapped.clear();

    vulkanRendererContext.uniformBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
    vulkanRendererContext.uniformBuffersMemory.reserve(MAX_FRAMES_IN_FLIGHT);
    vulkanRendererContext.uniformBuffersMapped.reserve(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        BufferWithMemory bufferWithMemory = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);

        vulkanRendererContext.uniformBuffers.emplace_back(std::move(bufferWithMemory.buffer));
        vulkanRendererContext.uniformBuffersMemory.emplace_back(std::move(bufferWithMemory.memory));

        vulkanRendererContext.uniformBuffersMapped.push_back(
            vulkanRendererContext.uniformBuffersMemory.back().mapMemory(0, bufferSize));
    }
}

void createCommandBuffers()
{
    vulkanRendererContext.commandBuffers.clear();

    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = *vulkanContext.commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

    vulkanRendererContext.commandBuffers = vk::raii::CommandBuffers{vulkanContext.device, allocInfo};
}

void setupRenderer()
{
    createDescriptorSetLayout();
    createGraphicsPipeline();

    createCommandBuffers();

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
}
