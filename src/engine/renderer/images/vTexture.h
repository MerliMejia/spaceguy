#pragma once

#include "stb_image.h"
#include "vImage.h"

namespace Renderer {
namespace Images {
struct VTexture {
  VImage vImage = {};
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t index = -1;

  inline void create(const std::string path, VDevice &vDevice,
                     vk::raii::CommandPool &commandPool,
                     vk::raii::Queue &graphicsQueue) {
    int loadedWidth = 0;
    int loadedHeight = 0;
    int sourceChannelCount = 0;

    stbi_uc *pixels = stbi_load(path.c_str(), &loadedWidth, &loadedHeight,
                                &sourceChannelCount, STBI_rgb_alpha);

    if (pixels == nullptr) {
      throw std::runtime_error("failed to decode texture image");
    }

    if (loadedWidth <= 0 || loadedHeight <= 0) {
      stbi_image_free(pixels);
      throw std::runtime_error("texture has invalid dimensions");
    }

    this->width = static_cast<uint32_t>(loadedWidth);
    this->height = static_cast<uint32_t>(loadedHeight);

    // Will be used to transfer pixels from file to it and then to be sampled by
    // the shader.
    vImage.usage =
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;

    vImage.init(width, height, vDevice);

    const vk::DeviceSize imageByteSize =
        static_cast<vk::DeviceSize>(this->width) *
        static_cast<vk::DeviceSize>(this->height) * 4;

    BufferAllocation staging = createBuffer(
        vDevice, imageByteSize, vk::BufferUsageFlagBits::eTransferSrc,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

    void *mapped = staging.memory.mapMemory(0, imageByteSize);
    std::memcpy(mapped, pixels, static_cast<std::size_t>(imageByteSize));
    staging.memory.unmapMemory();

    stbi_image_free(pixels);
    pixels = nullptr;

    vk::CommandBufferAllocateInfo commandBufferInfo{
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };

    auto uploadCommandBuffers =
        vk::raii::CommandBuffers(vDevice.device, commandBufferInfo);

    vk::raii::CommandBuffer uploadCommandBuffer =
        std::move(uploadCommandBuffers.front());

    vk::CommandBufferBeginInfo beginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };

    uploadCommandBuffer.begin(beginInfo);

    vImage.transition(
        vk::PipelineStageFlagBits2::eNone, vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite, vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal, uploadCommandBuffer);

    vk::BufferImageCopy copyRegion{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset = {0, 0, 0},
        .imageExtent =
            {
                .width = width,
                .height = height,
                .depth = 1,
            },
    };

    uploadCommandBuffer.copyBufferToImage(*staging.buffer, *vImage.image,
                                          vk::ImageLayout::eTransferDstOptimal,
                                          copyRegion);

    // In this case, we transition the image again to eShaderReadOnlyOptimal
    // with eFragmentShader and eShaderRead because a texture will always be
    // sampled by the shader.
    vImage.transition(
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderRead, vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal, uploadCommandBuffer);

    uploadCommandBuffer.end();

    const vk::CommandBuffer rawCommandBuffer = *uploadCommandBuffer;

    vk::SubmitInfo submitInfo{
        .commandBufferCount = 1,
        .pCommandBuffers = &rawCommandBuffer,
    };

    // Finish uploading the image.
    graphicsQueue.submit(submitInfo, nullptr);
    graphicsQueue.waitIdle();
  }
};
} // namespace Images
} // namespace Renderer
