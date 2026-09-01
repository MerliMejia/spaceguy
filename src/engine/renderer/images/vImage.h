#pragma once

#include "../bufferUtils.h"
#include "../vDevice.h"
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace Renderer {
namespace Images {

static void transitionImage(
    vk::Image &image, vk::PipelineStageFlags2 initialPlace,
    vk::AccessFlags2 initialAccess, vk::PipelineStageFlags2 newPlace,
    vk::AccessFlags2 newAccess, vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout, vk::raii::CommandBuffer &commandBuffer,
    vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor) {

  const vk::ImageSubresourceRange colorSubresource{
      .aspectMask = aspectMask,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };

  vk::ImageMemoryBarrier2 prepareForCopy{
      .srcStageMask = initialPlace,
      .srcAccessMask = initialAccess,
      .dstStageMask = newPlace,
      .dstAccessMask = newAccess,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = colorSubresource,
  };

  vk::DependencyInfo prepareDependency{
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &prepareForCopy,
  };

  commandBuffer.pipelineBarrier2(prepareDependency);
}

struct VImage {
  vk::raii::DeviceMemory memory{nullptr};
  vk::raii::Image image{nullptr};
  vk::raii::ImageView view{nullptr};

  vk::ImageUsageFlags usage;
  vk::Format format = vk::Format::eR8G8B8A8Srgb;
  vk::ImageType type = vk::ImageType::e2D;
  vk::ImageViewType viewType = vk::ImageViewType::e2D;
  vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;

  inline void init(uint32_t width, uint32_t height, VDevice &vDevice) {
    vk::ImageCreateInfo imageInfo{
        .imageType = type,
        .format = format,
        .extent =
            {
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height),
                .depth = 1,
            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    width = static_cast<uint32_t>(width);
    height = static_cast<uint32_t>(height);
    image = vk::raii::Image(vDevice.device, imageInfo);

    const vk::MemoryRequirements requirements = image.getMemoryRequirements();

    vk::MemoryAllocateInfo allocationInfo{
        .allocationSize = requirements.size,
        .memoryTypeIndex =
            findMemoryType(vDevice.physicalDevice, requirements.memoryTypeBits,
                           vk::MemoryPropertyFlagBits::eDeviceLocal),
    };

    memory = vk::raii::DeviceMemory(vDevice.device, allocationInfo);
    image.bindMemory(*memory, 0);

    vk::ImageViewCreateInfo viewInfo{
        .image = *image,
        .viewType = viewType,
        .format = format,
        .subresourceRange =
            {
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    view = vk::raii::ImageView(vDevice.device, viewInfo);
  }

  inline void transition(
      vk::PipelineStageFlags2 initialPlace, vk::AccessFlags2 initialAccess,
      vk::PipelineStageFlags2 newPlace, vk::AccessFlags2 newAccess,
      vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
      vk::raii::CommandBuffer &commandBuffer,
      vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor) {

    vk::Image cHandle = static_cast<vk::Image>(*image);
    transitionImage(cHandle, initialPlace, initialAccess, newPlace, newAccess,
                    oldLayout, newLayout, commandBuffer, aspectMask);
  }
};

} // namespace Images
} // namespace Renderer
