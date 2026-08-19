#pragma once

#include "bufferUtils.h"
#include "glm/fwd.hpp"
#include "memoryUtils.h"
#include "vSwapChain.h"
#include "vulkan/vulkan.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "../../utils/file.h"
#include <chrono>
#define GLM_FORCE_RADIANS
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

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {.binding = 0,
            .stride = sizeof(DefaultVertex),
            .inputRate = vk::VertexInputRate::eVertex};
  }
  static std::array<vk::VertexInputAttributeDescription, 2>
  getAttributeDescriptions() {
    return {{{.location = 0,
              .binding = 0,
              .format = vk::Format::eR32G32Sfloat,
              .offset = offsetof(DefaultVertex, pos)},
             {.location = 1,
              .binding = 0,
              .format = vk::Format::eR32G32B32Sfloat,
              .offset = offsetof(DefaultVertex, color)}}};
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

struct GlobalUniformBankBuffer {
  std::array<glm::vec4, Renderer::Shaders::UniformBank::TOTAL_SLOTS> data;
};

struct PushConstants {
  uint32_t modelIndex;
};

struct RenderNode {
  vk::raii::ShaderModule shaderModule = nullptr;
  std::vector<ShaderCreateInfo> shaderCreateInfos;
  vk::raii::PipelineLayout pipelineLayout = nullptr;
  vk::raii::Pipeline graphicsPipeline = nullptr;
  vk::raii::CommandPool commandPool = nullptr;
  std::vector<vk::raii::CommandBuffer> commandBuffers;
  vk::raii::Buffer vertexBuffer = nullptr;
  vk::raii::DeviceMemory vertexBufferMemory = nullptr;
  vk::raii::Buffer indexBuffer = nullptr;
  vk::raii::DeviceMemory indexBufferMemory = nullptr;
  std::vector<uint32_t> indices;

  // Everything we need for a global uniform bank
  vk::raii::DescriptorSetLayout globalUniformBankDescriptorSetLayout = nullptr;
  GlobalUniformBankBuffer globalUniformBufferData{};
  std::vector<BufferAllocationWithMapped> globalUniformBankBuffers;
  vk::raii::DescriptorPool globalUniformBankDescriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> globalUniformBankDescriptorSets;

  PushConstants pushConstants{};

  void step1_initShaders(vk::raii::Device &device,
                         step1_initShadersProps props) {

    // TODO - This is for testing.
    glm::mat4 model = rotate(glm::mat4(1.0f), glm::radians(90.0f),
                             glm::vec3(0.0f, 0.0f, 1.0f));

    const uint32_t modelIndex = 0;

    Renderer::Memory::updateMat4ByIndex(modelIndex,
                                        globalUniformBufferData.data, model);

    pushConstants.modelIndex = modelIndex;

    shaderModule = createShaderModule(readFile(props.shaderFile), device);
    shaderCreateInfos = std::move(props.shaderCreateInfos);
  }

  template <VertexType T>
  void step_1_1_createAndFillVertexBuffer(
      // TODO - Default vertex for now, will change as this grows
      std::vector<T> incomingVertices, VDevice &vDevice) {

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
                                      VDevice &vDevice) {

    indices = std::move(incomingIndices);
    BufferAllocation allocation = createDeviceLocalBuffer(
        vDevice, commandPool, std::span<const uint32_t>{indices},
        vk::BufferUsageFlagBits::eIndexBuffer);

    indexBuffer = std::move(allocation.buffer);
    indexBufferMemory = std::move(allocation.memory);
  }

  void step_1_3_createUniformBuffers(VDevice &vDevice) {
    // For now, only the uniform bank.
    // We need one per each frame in flight.
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

      vk::DeviceSize bufferSize = sizeof(GlobalUniformBankBuffer);
      BufferAllocationWithMapped newUniformBuffer{};

      BufferAllocation alloc = createBuffer(
          vDevice, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent);

      newUniformBuffer.buffer = std::move(alloc.buffer);
      newUniformBuffer.memory = std::move(alloc.memory);
      newUniformBuffer.mapped =
          newUniformBuffer.memory.mapMemory(0, bufferSize);

      globalUniformBankBuffers.emplace_back(std::move(newUniformBuffer));
    }
  }

  // Depends A LOT of the shader, need to create a way to define this as with
  // the shader.
  void step_1_4_createDescriptorSetLayout(vk::raii::Device &device) {
    vk::DescriptorSetLayoutBinding uboLayoutBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex};
    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = 1, .pBindings = &uboLayoutBinding};
    globalUniformBankDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(device, layoutInfo);
  }

  // Right now this is only for the global uniforms bank. This will depend
  // on the other data we're mapping to the gpu and if that data needs
  // to be updated every frame.
  void step_1_5_createDescriptorPool(vk::raii::Device &device) {
    vk::DescriptorPoolSize poolSize{.type = vk::DescriptorType::eUniformBuffer,
                                    .descriptorCount = MAX_FRAMES_IN_FLIGHT};

    vk::DescriptorPoolCreateInfo poolInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize};

    globalUniformBankDescriptorPool =
        vk::raii::DescriptorPool(device, poolInfo);
  }

  // Dito above
  void step_1_6_allocateDescriptorSets(vk::raii::Device &device) {
    std::vector<vk::DescriptorSetLayout> layouts(
        MAX_FRAMES_IN_FLIGHT, *globalUniformBankDescriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = globalUniformBankDescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()};

    globalUniformBankDescriptorSets = device.allocateDescriptorSets(allocInfo);
  }

  // Dito above
  void step_1_7_configureDescriptorSets(vk::raii::Device &device) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DescriptorBufferInfo bufferInfo{
          .buffer = globalUniformBankBuffers[i].buffer,
          .offset = 0,
          .range = sizeof(GlobalUniformBankBuffer)};
      vk::WriteDescriptorSet descriptorWrite{
          .dstSet = globalUniformBankDescriptorSets[i],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBuffer,
          .pBufferInfo = &bufferInfo};

      device.updateDescriptorSets(descriptorWrite, {});
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

    // TODO - Flags should be defined by the person configuring the node.
    vk::PushConstantRange pushConstantRange;
    pushConstantRange.setStageFlags(vk::ShaderStageFlagBits::eVertex)
        .setOffset(0)
        .setSize(sizeof(PushConstants));

    // We will definetely add more set layouts.
    // Also, I think here's where I add the push constants.
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &*globalUniformBankDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange};

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
  void step3_initCommandBuffer(uint32_t queueIndex, vk::raii::Device &device) {
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
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto thisCurrentTime = std::chrono::high_resolution_clock::now();
    float time =
        std::chrono::duration<float>(thisCurrentTime - startTime).count();

    glm::mat4 model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                             glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 view =
        lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
               glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 proj = glm::perspective(
        glm::radians(45.0f), static_cast<float>(800) / static_cast<float>(600),
        0.1f, 10.0f);

    proj[1][1] *= -1;

    Renderer::Memory::updateMat4ByIndex(0, globalUniformBufferData.data, model);
    Renderer::Memory::updateMat4ByIndex(
        Renderer::Shaders::UniformBank::viewIndex, globalUniformBufferData.data,
        view);
    Renderer::Memory::updateMat4ByIndex(
        Renderer::Shaders::UniformBank::projIndex, globalUniformBufferData.data,
        proj);

    memcpy(globalUniformBankBuffers[frameIndex].mapped,
           &globalUniformBufferData, sizeof(globalUniformBufferData));
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
        *globalUniformBankDescriptorSets[frameIndex], nullptr);

    // TODO - The flags should be defined?
    commandBuffers[frameIndex].pushConstants(
        *pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
        sizeof(PushConstants), &pushConstants);

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
