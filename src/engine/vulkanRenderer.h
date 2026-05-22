#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include <vector>

#include "vulkanGlobals.h"

// Will store/handle every vulkan stuff that can change depending of how we decide that the renderer
// will render stuff.

struct VulkanRendererContext
{
    // Per "way of rendering" / shader / contract. Set normally once.
    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    vk::raii::DescriptorPool descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;
    vk::raii::PipelineLayout pipelineLayout{nullptr};
    vk::raii::Pipeline graphicsPipeline{nullptr};

    // Per frame?
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void *> uniformBuffersMapped;
};

extern VulkanRendererContext vulkanRendererContext;

void setupRenderer();