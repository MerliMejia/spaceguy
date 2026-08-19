#pragma once

#include "vDevice.h"

#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Renderer {

// Memory must be declared before the buffer so destruction happens in the
// correct order: buffer first, then its bound memory.
struct BufferAllocation {
  vk::raii::DeviceMemory memory = nullptr;
  vk::raii::Buffer buffer = nullptr;
};

struct BufferAllocationWithMapped : public BufferAllocation {
  void *mapped = nullptr;
};

inline uint32_t findMemoryType(const vk::raii::PhysicalDevice &physicalDevice,
                               uint32_t typeFilter,
                               vk::MemoryPropertyFlags requiredProperties) {

  const vk::PhysicalDeviceMemoryProperties memoryProperties =
      physicalDevice.getMemoryProperties();

  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
    const bool typeMatches = (typeFilter & (1u << i)) != 0;
    const bool propertiesMatch =
        (memoryProperties.memoryTypes[i].propertyFlags & requiredProperties) ==
        requiredProperties;

    if (typeMatches && propertiesMatch) {
      return i;
    }
  }

  throw std::runtime_error("failed to find suitable Vulkan memory type");
}

inline BufferAllocation createBuffer(VDevice &vDevice, vk::DeviceSize size,
                                     vk::BufferUsageFlags usage,
                                     vk::MemoryPropertyFlags memoryProperties) {

  if (size == 0) {
    throw std::runtime_error("cannot create a zero-sized Vulkan buffer");
  }

  vk::BufferCreateInfo bufferInfo{
      .size = size,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  vk::raii::Buffer buffer{vDevice.device, bufferInfo};

  const vk::MemoryRequirements requirements = buffer.getMemoryRequirements();

  vk::MemoryAllocateInfo allocationInfo{
      .allocationSize = requirements.size,
      .memoryTypeIndex =
          findMemoryType(vDevice.physicalDevice, requirements.memoryTypeBits,
                         memoryProperties),
  };

  vk::raii::DeviceMemory memory{
      vDevice.device,
      allocationInfo,
  };

  buffer.bindMemory(*memory, 0);

  return BufferAllocation{
      .memory = std::move(memory),
      .buffer = std::move(buffer),
  };
}

inline void copyBuffer(VDevice &vDevice,
                       const vk::raii::CommandPool &commandPool,
                       const vk::raii::Buffer &source,
                       const vk::raii::Buffer &destination,
                       vk::DeviceSize size) {

  vk::CommandBufferAllocateInfo allocationInfo{
      .commandPool = *commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };

  std::vector<vk::raii::CommandBuffer> commandBuffers =
      vDevice.device.allocateCommandBuffers(allocationInfo);

  vk::raii::CommandBuffer commandBuffer = std::move(commandBuffers.front());

  vk::CommandBufferBeginInfo beginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  };

  commandBuffer.begin(beginInfo);

  vk::BufferCopy copyRegion{
      .srcOffset = 0,
      .dstOffset = 0,
      .size = size,
  };

  commandBuffer.copyBuffer(*source, *destination, copyRegion);

  commandBuffer.end();

  const vk::CommandBuffer rawCommandBuffer = *commandBuffer;

  vk::SubmitInfo submitInfo{
      .commandBufferCount = 1,
      .pCommandBuffers = &rawCommandBuffer,
  };

  vDevice.graphicsQueue.submit(submitInfo, nullptr);
  vDevice.graphicsQueue.waitIdle();
}

inline BufferAllocation
createDeviceLocalBuffer(VDevice &vDevice,
                        const vk::raii::CommandPool &commandPool,
                        const void *sourceData, vk::DeviceSize size,
                        vk::BufferUsageFlags finalUsage) {

  if (sourceData == nullptr) {
    throw std::runtime_error("cannot upload null buffer data");
  }

  BufferAllocation staging =
      createBuffer(vDevice, size, vk::BufferUsageFlagBits::eTransferSrc,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void *mappedMemory = staging.memory.mapMemory(0, size);
  std::memcpy(mappedMemory, sourceData, static_cast<std::size_t>(size));
  staging.memory.unmapMemory();

  BufferAllocation deviceLocal = createBuffer(
      vDevice, size, finalUsage | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal);

  copyBuffer(vDevice, commandPool, staging.buffer, deviceLocal.buffer, size);

  return deviceLocal;
}

template <typename T>
inline BufferAllocation createDeviceLocalBuffer(
    VDevice &vDevice, const vk::raii::CommandPool &commandPool,
    std::span<const T> source, vk::BufferUsageFlags finalUsage) {

  static_assert(std::is_trivially_copyable_v<T>,
                "GPU buffer elements must be trivially copyable");

  if (source.empty()) {
    throw std::runtime_error("cannot upload an empty buffer");
  }

  const vk::DeviceSize size =
      sizeof(T) * static_cast<vk::DeviceSize>(source.size());

  return createDeviceLocalBuffer(vDevice, commandPool, source.data(), size,
                                 finalUsage);
}

} // namespace Renderer
