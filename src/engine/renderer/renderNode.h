#pragma once

#include "bufferUtils.h"
#include "glm/fwd.hpp"
#include "images/vImageManager.h"
#include "renderGraphUtils.h"
#include "vSwapChain.h"
#include "vulkan/vulkan.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "../../utils/file.h"
#define GLM_FORCE_RADIANS
#include "images/vTexture.h"
#include "shaders.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace {
vk::raii::ShaderModule createShaderModule(const std::vector<char> &code,
                                          vk::raii::Device &device) {
  vk::ShaderModuleCreateInfo createInfo{
      .codeSize = code.size(),
      .pCode = reinterpret_cast<const uint32_t *>(code.data())};

  return vk::raii::ShaderModule{device, createInfo};
}
} // namespace

namespace Renderer {
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

template <typename T>
concept VertexType = requires {
  T::getBindingDescription();
  T::getAttributeDescriptions();
};

struct DefaultVertex {
  glm::vec2 pos;
  glm::vec3 color;
  glm::vec2 uv;

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {.binding = 0,
            .stride = sizeof(DefaultVertex),
            .inputRate = vk::VertexInputRate::eVertex};
  }
  static std::array<vk::VertexInputAttributeDescription, 3>
  getAttributeDescriptions() {
    return {{
        {.location = 0,
         .binding = 0,
         .format = vk::Format::eR32G32Sfloat,
         .offset = offsetof(DefaultVertex, pos)},
        {.location = 1,
         .binding = 0,
         .format = vk::Format::eR32G32B32Sfloat,
         .offset = offsetof(DefaultVertex, color)},
        {.location = 2,
         .binding = 0,
         .format = vk::Format::eR32G32Sfloat,
         .offset = offsetof(DefaultVertex, uv)},
    }};
  }
};

enum class ShaderType {
  None,
  Vertex,
  Fragment,
};

struct ShaderCreateInfo {
  ShaderType type = ShaderType::None;
  std::string name = "";
};

struct step1_initShadersProps {
  std::vector<ShaderCreateInfo> shaderCreateInfos;
  std::string shaderFile;
};

struct step2_pipelineConfigurationProps {
  vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
  //... More stuff when dealing with more stuff
};

struct RenderNode {
  vk::raii::ShaderModule shaderModule = nullptr;
  std::vector<ShaderCreateInfo> shaderCreateInfos;
  vk::raii::PipelineLayout pipelineLayout = nullptr;
  vk::raii::Pipeline graphicsPipeline = nullptr;
  std::vector<vk::raii::CommandBuffer> commandBuffers;
  vk::raii::Buffer vertexBuffer = nullptr;
  vk::raii::DeviceMemory vertexBufferMemory = nullptr;
  vk::raii::Buffer indexBuffer = nullptr;
  vk::raii::DeviceMemory indexBufferMemory = nullptr;
  std::vector<uint32_t> indices;

  Renderer::RenderGraph::Context *renderGraphContext = nullptr;
  bool usePushConstants = false;
  bool updateUniforms = false;

  void preInit(Renderer::RenderGraph::Context &ctx) {
    renderGraphContext = &ctx;
  }

  void step1_initShaders(vk::raii::Device &device,
                         step1_initShadersProps props) {
    shaderModule = createShaderModule(readFile(props.shaderFile), device);
    shaderCreateInfos = std::move(props.shaderCreateInfos);
  }

  template <VertexType T>
  void step_1_1_createAndFillVertexBuffer(std::vector<T> incomingVertices,
                                          VDevice &vDevice,
                                          vk::raii::CommandPool &commandPool) {

    vk::CommandPoolCreateInfo poolInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = vDevice.queueIndex,
    };

    commandPool = vk::raii::CommandPool{
        vDevice.device,
        poolInfo,
    };

    BufferAllocation allocation = createDeviceLocalBuffer(
        vDevice, commandPool, std::span<const T>{incomingVertices},
        vk::BufferUsageFlagBits::eVertexBuffer);

    vertexBuffer = std::move(allocation.buffer);
    vertexBufferMemory = std::move(allocation.memory);
  }

  void
  step_1_2_createAndFillIndicesBuffer(std::vector<uint32_t> incomingIndices,
                                      VDevice &vDevice,
                                      vk::raii::CommandPool &commandPool) {

    indices = std::move(incomingIndices);
    BufferAllocation allocation = createDeviceLocalBuffer(
        vDevice, commandPool, std::span<const uint32_t>{indices},
        vk::BufferUsageFlagBits::eIndexBuffer);

    indexBuffer = std::move(allocation.buffer);
    indexBufferMemory = std::move(allocation.memory);
  }

  void step_1_3_createUniformBuffers(VDevice &vDevice) {
    // For now 1 per frame in flight. At some point I may want something that is
    // more static.
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

      vk::DeviceSize bufferSize =
          sizeof(RenderGraph::Context::GlobalUniformBankBuffer);
      BufferAllocationWithMapped newUniformBuffer{};

      BufferAllocation alloc = createBuffer(
          vDevice, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent);

      newUniformBuffer.buffer = std::move(alloc.buffer);
      newUniformBuffer.memory = std::move(alloc.memory);
      newUniformBuffer.mapped =
          newUniformBuffer.memory.mapMemory(0, bufferSize);

      renderGraphContext->globalUniformBankBuffers.emplace_back(
          std::move(newUniformBuffer));
    }
  }

  // I'll manually define this in a way that each node can use it however it
  // wants. Right now 0,0 -> uniform bank, 1,0 -> sampler, 2,0 0 -> texture
  // bank.
  void step_1_4_createDescriptorSetLayout(vk::raii::Device &device) {
    std::array<vk::DescriptorSetLayoutBinding, 3> bingdings{
        vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex},
        vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment},
        vk::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .descriptorCount = Shaders::MAX_TEXTURES,
            .stageFlags = vk::ShaderStageFlagBits::eFragment}};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = static_cast<uint32_t>(bingdings.size()),
        .pBindings = bingdings.data()};
    renderGraphContext->defaultDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(device, layoutInfo);
  }

  void step_1_5_createDescriptorPool(vk::raii::Device &device) {
    std::array<vk::DescriptorPoolSize, 3> poolSizes = {
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer,
                               .descriptorCount = MAX_FRAMES_IN_FLIGHT},
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampler,
                               .descriptorCount = MAX_FRAMES_IN_FLIGHT},
        // Each frame in flight to have 20 sampled textures.
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampledImage,
                               .descriptorCount = MAX_FRAMES_IN_FLIGHT *
                                                  Shaders::MAX_TEXTURES}};

    vk::DescriptorPoolCreateInfo poolInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()};

    renderGraphContext->defaultDescriptorPool =
        vk::raii::DescriptorPool(device, poolInfo);
  }

  void step_1_6_allocateDescriptorSets(vk::raii::Device &device) {
    std::vector<vk::DescriptorSetLayout> layouts(
        MAX_FRAMES_IN_FLIGHT, *renderGraphContext->defaultDescriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = renderGraphContext->defaultDescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()};

    renderGraphContext->defaultDescriptorSets =
        device.allocateDescriptorSets(allocInfo);
  }

  void step_1_7_configureDescriptorSets(
      vk::raii::Device &device, Renderer::Images::VManager &vTextureManager) {

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DescriptorBufferInfo bufferInfo{
          .buffer = renderGraphContext->globalUniformBankBuffers[i].buffer,
          .offset = 0,
          .range = sizeof(RenderGraph::Context::GlobalUniformBankBuffer)};

      vk::DescriptorImageInfo samplerInfo{
          .sampler = *vTextureManager.sampler,
          .imageView = {},
          .imageLayout = vk::ImageLayout::eUndefined,
      };

      std::array<vk::DescriptorImageInfo, Shaders::MAX_TEXTURES> imageInfos;

      for (int i = 0; i < imageInfos.size(); i++) {
        imageInfos[i] = vk::DescriptorImageInfo{
            .sampler = {},
            .imageView = *vTextureManager.textures[i]->vImage.view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
      }

      std::array<vk::WriteDescriptorSet, 3> writes{
          vk::WriteDescriptorSet{
              .dstSet = *renderGraphContext->defaultDescriptorSets[i],
              .dstBinding = 0,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = vk::DescriptorType::eUniformBuffer,
              .pBufferInfo = &bufferInfo,
          },
          vk::WriteDescriptorSet{
              .dstSet = *renderGraphContext->defaultDescriptorSets[i],
              .dstBinding = 1,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = vk::DescriptorType::eSampler,
              .pImageInfo = &samplerInfo,
          },
          {
              .dstSet = *renderGraphContext->defaultDescriptorSets[i],
              .dstBinding = 2,
              .dstArrayElement = 0,
              .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
              .descriptorType = vk::DescriptorType::eSampledImage,
              .pImageInfo = imageInfos.data(),
          }};

      device.updateDescriptorSets(writes, {});
    }
  }

  template <VertexType T>
  void
  step2_initPipelineConfiguration(vk::raii::Device &device,
                                  vk::SurfaceFormatKHR &swapChainSurfaceFormat,
                                  step2_pipelineConfigurationProps props) {

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    for (const ShaderCreateInfo &info : shaderCreateInfos) {
      if (info.type == ShaderType::Vertex) {
        shaderStages.push_back(vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = *shaderModule,
            .pName = info.name.c_str()});
      } else if (info.type == ShaderType::Fragment) {
        shaderStages.push_back(vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = *shaderModule,
            .pName = info.name.c_str()});
      }
    }

    auto bindingDescription = T::getBindingDescription();
    auto attributeDescriptions = T::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology =
                                                               props.topology};
    vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                      .scissorCount = 1};

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False};

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                                   vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    vk::PushConstantRange pushConstantRange;

    // We will definetely add more set layouts.
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &*renderGraphContext->defaultDescriptorSetLayout};

    if (usePushConstants) {
      pushConstantRange
          .setStageFlags(vk::ShaderStageFlagBits::eVertex |
                         vk::ShaderStageFlagBits::eFragment)
          .setOffset(0)
          .setSize(sizeof(Shaders::PushConstantsBank::PushConstantData));

      pipelineLayoutInfo.pushConstantRangeCount = 1;
      pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    }

    pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

    vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                       vk::PipelineRenderingCreateInfo>
        pipelineCreateInfoChain = {
            {.stageCount = static_cast<std::uint32_t>(shaderStages.size()),
             .pStages = shaderStages.data(),
             .pVertexInputState = &vertexInputInfo,
             .pInputAssemblyState = &inputAssembly,
             .pViewportState = &viewportState,
             .pRasterizationState = &rasterizer,
             .pMultisampleState = &multisampling,
             .pColorBlendState = &colorBlending,
             .pDynamicState = &dynamicState,
             .layout = *pipelineLayout,
             .renderPass = nullptr},
            {.colorAttachmentCount = 1,
             .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

    graphicsPipeline = vk::raii::Pipeline(
        device, nullptr,
        pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
  }

  // The initial image layout transition is from eUndefined to
  // eColorAttachmentOptimal assumming that by default we always want to draw a
  // color attachment. This should change later so I can define what's the
  // initial layout transition. Probably eUndefined to depth, stencil, other?
  void step3_initCommandBuffer(uint32_t queueIndex, vk::raii::Device &device,
                               vk::raii::CommandPool &commandPool) {
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = Renderer::MAX_FRAMES_IN_FLIGHT};

    commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
  }

  // For now, just the global uniform bank.
  // I think it depends on the node to update these.
  // One node can do this but not all of the nodes.
  void perFrame1_updateUniformBuffers(uint32_t frameIndex) {
    memcpy(renderGraphContext->globalUniformBankBuffers[frameIndex].mapped,
           &renderGraphContext->globalUniformBufferData,
           sizeof(renderGraphContext->globalUniformBufferData));
  }

  // Will definetly change. This is where each render node should define what to
  // do when rendering each frame. Will it get an image as input? What's the
  // output? which resources will bind?
  void perFrame2_recordCommandBuffer(Renderer::VSwapChain &vSwapChain,
                                     uint32_t imageIndex, uint32_t frameIndex) {
    commandBuffers[frameIndex].begin({});

    // This whole block is for "transitioning" images and it's better to write
    // it down manually to understand what it does. Before starting rendering,
    // transition the swapchain image to
    // vk::ImageLayout::eColorAttachmentOptimal
    vk::ImageMemoryBarrier2 barrier = {
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = {}, // Because it doesn't have to wait for something to
                             // finish in this case?
        .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vSwapChain.swapChainImages[imageIndex],
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    vk::DependencyInfo dependency_info = {.dependencyFlags = {},
                                          .imageMemoryBarrierCount = 1,
                                          .pImageMemoryBarriers = &barrier};
    commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
    // This whole block is for "transitioning" images and it's better to write
    // it down manually to understand what it does.

    vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::RenderingAttachmentInfo attachmentInfo = {
        .imageView = vSwapChain.swapChainImageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor};
    vk::RenderingInfo renderingInfo = {
        .renderArea = {.offset = {0, 0}, .extent = vSwapChain.swapChainExtent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo};

    commandBuffers[frameIndex].beginRendering(renderingInfo);
    commandBuffers[frameIndex].bindPipeline(vk::PipelineBindPoint::eGraphics,
                                            *graphicsPipeline);
    commandBuffers[frameIndex].setViewport(
        0, vk::Viewport(0.0f, 0.0f,
                        static_cast<float>(vSwapChain.swapChainExtent.width),
                        static_cast<float>(vSwapChain.swapChainExtent.height),
                        0.0f, 1.0f));
    commandBuffers[frameIndex].setScissor(
        0, vk::Rect2D(vk::Offset2D(0, 0), vSwapChain.swapChainExtent));

    commandBuffers[frameIndex].bindVertexBuffers(0, *vertexBuffer, {0});
    commandBuffers[frameIndex].bindIndexBuffer(*indexBuffer, 0,
                                               vk::IndexType::eUint32);
    commandBuffers[frameIndex].bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
        *renderGraphContext->defaultDescriptorSets[frameIndex], nullptr);

    if (usePushConstants) {
      commandBuffers[frameIndex].pushConstants(
          *pipelineLayout,
          vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
          0, sizeof(Shaders::PushConstantsBank::PushConstantData),
          &renderGraphContext->pushConstantBank);
    }

    commandBuffers[frameIndex].drawIndexed(
        static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    commandBuffers[frameIndex].endRendering();

    // After rendering, transition the swapchain image to
    // vk::ImageLayout::ePresentSrcKHR
    vk::ImageMemoryBarrier2 barrier2 = {
        .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlagBits2::
            eColorAttachmentWrite, // Now it needs to wait for
                                   // eColorAttachmentWrite because before this
                                   // we were transitioning from undefined to
                                   // color attachment.
        .dstStageMask =
            vk::PipelineStageFlagBits2::eBottomOfPipe, // I guess it makes
                                                       // sense?
        .dstAccessMask = {},                           // No idea
        .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .newLayout = vk::ImageLayout::ePresentSrcKHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vSwapChain.swapChainImages[imageIndex],
        .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    vk::DependencyInfo dependency_info2 = {.dependencyFlags = {},
                                           .imageMemoryBarrierCount = 1,
                                           .pImageMemoryBarriers = &barrier2};
    commandBuffers[frameIndex].pipelineBarrier2(dependency_info2);

    commandBuffers[frameIndex].end();
  }
};
} // namespace Renderer
