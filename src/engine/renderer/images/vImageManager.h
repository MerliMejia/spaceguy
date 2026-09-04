#pragma once

#include "../../shaders/v2/banks_shared.h"
#include "vTexture.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace Renderer {
namespace Images {

struct VManager {
  std::array<std::unique_ptr<VTexture>, SG_MAX_TEXTURES> ownedTextures{};
  std::array<VTexture *, SG_MAX_TEXTURES> textures{};

  vk::raii::Sampler sampler{nullptr};
  VTexture fallbackTexture{};

  std::size_t nextFreeIndex = 0;

  void init(VDevice &vDevice, vk::raii::CommandPool &commandPool) {
    vk::SamplerCreateInfo samplerInfo{
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = vk::False,
        .maxAnisotropy = 1.0f,
        .compareEnable = vk::False,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = vk::False,
    };

    sampler = vk::raii::Sampler(vDevice.device, samplerInfo);

    fallbackTexture.create("assets/tiny.jpg", vDevice, commandPool,
                           vDevice.graphicsQueue);

    textures.fill(&fallbackTexture);
  }

  VTexture *createTexture(const std::string &path, VDevice &vDevice,
                          vk::raii::CommandPool &commandPool,
                          vk::raii::Queue &graphicsQueue) {
    if (nextFreeIndex >= textures.size()) {
      throw std::runtime_error("texture bank is full");
    }

    const std::size_t index = nextFreeIndex;

    auto newTexture = std::make_unique<VTexture>();

    newTexture->index = static_cast<uint32_t>(index);
    newTexture->create(path, vDevice, commandPool, graphicsQueue);

    VTexture *result = newTexture.get();

    ownedTextures[index] = std::move(newTexture);
    textures[index] = result;

    ++nextFreeIndex;

    return result;
  }
};

} // namespace Images
} // namespace Renderer
