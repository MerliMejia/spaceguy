#pragma once

#include <string>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "../../utils/file.h"
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
};

struct step2_pipelineConfigurationProps {
  vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
  //... More stuff when dealing with more stuff
};

struct RenderNode {
  vk::raii::ShaderModule shaderModule = nullptr;
  std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
  vk::raii::PipelineLayout pipelineLayout = nullptr;
  vk::raii::Pipeline graphicsPipeline = nullptr;

  void step1_initShaders(vk::raii::Device &device,
                         step1_initShadersProps props) {
    shaderModule = createShaderModule(readFile("shaders/slang.spv"), device);

    for (ShaderCreateInfo info : props.shaderCreateInfos) {
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
  }

  // TODO - upgrade this later
  void step2_pipelineConfiguration(vk::raii::Device &device,
                                   vk::SurfaceFormatKHR &swapChainSurfaceFormat,
                                   step2_pipelineConfigurationProps props) {

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
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
};
} // namespace Renderer
