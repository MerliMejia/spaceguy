#include "buffers.h"
#include "../engine/vulkanBackend.h"

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

BufferWithMemory createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
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
