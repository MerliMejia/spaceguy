#pragma once

#include "../../../utils/file.h"
#include "../renderGraph/renderGraphUtils.h"
#include "../shaders/shaders.h"
#include "../vSwapChain.h"
#include "renderNodeUtils.h"
#include "vulkan/vulkan.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>


namespace Renderer {
struct ComputeNode {
  vk::raii::ShaderModule shaderModule = nullptr;
  std::vector<RenderNodeUtils::ShaderCreateInfo> shaderCreateInfos;
  vk::raii::PipelineLayout pipelineLayout = nullptr;
  vk::raii::Pipeline pipeline = nullptr;

  Renderer::RenderGraph::Context *renderGraphContext = nullptr;
  bool usePushConstants = false;
  std::function<void(Renderer::VSwapChain &vSwapChain, uint32_t imageIndex,
                     uint32_t frameIndex)>
      perFrameFunction;
  std::function<void()> updatePushConstants;

  void preInit(Renderer::RenderGraph::Context &ctx) {
    renderGraphContext = &ctx;
  }

  void step1_initShaders(vk::raii::Device &device, std::string shaderFile) {
    shaderModule =
        RenderNodeUtils::createShaderModule(readFile(shaderFile), device);
  }

  void step2_createPipelineLayout(vk::raii::Device &device) {

    vk::PushConstantRange pushConstantRange{
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
        .offset = 0,
        .size = sizeof(Shaders::PushConstantsBank::PushConstantData)};

    std::array<vk::DescriptorSetLayout, 2> setLayouts{
        *renderGraphContext->defaultDescriptorSetLayout,
        *renderGraphContext->computeDescriptorSetLayout};

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 2, .pSetLayouts = setLayouts.data()};

    if (usePushConstants) {
      pipelineLayoutInfo.pushConstantRangeCount = 1;
      pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    }

    pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);
  }

  void setPipeline(vk::raii::Device &device, std::string name) {

    vk::ComputePipelineCreateInfo pipelineCreateInfo{
        .stage =
            vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eCompute,
                .module = *shaderModule,
                .pName = name.c_str()},
        .layout = *pipelineLayout};

    pipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfo);
  }
};
} // namespace Renderer
