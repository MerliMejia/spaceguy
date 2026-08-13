#pragma once

#include "bufferUtils.h"
#include "vSwapChain.h"
#include <string>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "../../utils/file.h"
#include <glm/glm.hpp>
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
  // TODO - Default vertex for now, will change as this grows
  std::vector<Renderer::DefaultVertex> vertices;
  std::vector<uint32_t> indices;

  void step1_initShaders(vk::raii::Device &device,
                         step1_initShadersProps props) {
    shaderModule = createShaderModule(readFile(props.shaderFile), device);
    shaderCreateInfos = std::move(props.shaderCreateInfos);
  }

  // For now let's assume we'll always use the default vertex... Will definetly
  // chancge
  void step_1_1_createAndFillVertexBuffer(
      // TODO - Default vertex for now, will change as this grows
      std::vector<Renderer::DefaultVertex> incomingVertices, VDevice &vDevice) {

    vertices = std::move(incomingVertices);

    vk::CommandPoolCreateInfo poolInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = vDevice.queueIndex,
    };

    commandPool = vk::raii::CommandPool{
        vDevice.device,
        poolInfo,
    };

    BufferAllocation allocation = createDeviceLocalBuffer(
        vDevice, commandPool,
        std::span<const Renderer::DefaultVertex>{vertices},
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

  // TODO - upgrade this later
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

    // TODO - Default vertex for now, will change as this grows
    auto bindingDescription = Renderer::DefaultVertex::getBindingDescription();
    auto attributeDescriptions =
        Renderer::DefaultVertex::getAttributeDescriptions();

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
        .frontFace = vk::FrontFace::eClockwise,
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

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 0, .pushConstantRangeCount = 0};
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

  // Will definetly change. This is where each render node should define what to
  // do when rendering each frame. Will it get an image as input? What's the
  // output? which resources will bind?
  void perFrame1_recordCommandBuffer(Renderer::VSwapChain &vSwapChain,
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
