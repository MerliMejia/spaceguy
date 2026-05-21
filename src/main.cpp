#define GLFW_INCLUDE_NONE
#define VULKAN_HPP_NO_CONSTRUCTORS

#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>
#include <chrono>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <set>
#include <optional>
#include <utility>
#include <fstream>
#include <array>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <cstring>

#include "engine/vulkanBackend.h"

struct CameraBufferObject
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct ObjectPushConstants
{
    alignas(16) glm::mat4 model;
};

struct BufferWithMemory
{
    vk::raii::Buffer buffer;
    vk::raii::DeviceMemory memory;
};

struct Mesh
{
    uint32_t vertexCount;
};

struct Object3D
{
    const Mesh *mesh;
    glm::mat4 model;
};

Mesh triangleMesh{
    .vertexCount = 3};

vk::raii::PipelineLayout pipelineLayout{nullptr};
vk::raii::Pipeline graphicsPipeline{nullptr};

std::vector<vk::raii::CommandBuffer> commandBuffers;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
std::vector<vk::raii::Fence> inFlightFences;
uint32_t currentFrame = 0;

std::vector<vk::raii::Buffer> uniformBuffers;
std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
std::vector<void *> uniformBuffersMapped;

vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
vk::raii::DescriptorPool descriptorPool = nullptr;
std::vector<vk::raii::DescriptorSet> descriptorSets;

std::vector<Object3D> objects;

void createSceneObjects()
{
    objects.clear();

    objects.push_back(Object3D{
        .mesh = &triangleMesh,
        .model = glm::translate(glm::mat4(1.0f), glm::vec3(-0.75f, 0.0f, 0.0f)) *
                 glm::scale(glm::mat4(1.0f), glm::vec3(0.5f))});

    objects.push_back(Object3D{
        .mesh = &triangleMesh,
        .model =
            glm::translate(glm::mat4(1.0f), glm::vec3(0.75f, 0.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(0.5f))});
}

static std::vector<char> readFile(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

vk::raii::ShaderModule createShaderModule(const std::vector<char> &code)
{
    vk::ShaderModuleCreateInfo createInfo{
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t *>(code.data())};

    return vk::raii::ShaderModule{vulkanContext.device, createInfo};
}

void createGraphicsPipeline()
{
    auto vertShaderCode = readFile("shaders/triangle.vert.spv");
    auto fragShaderCode = readFile("shaders/triangle.frag.spv");

    vk::raii::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    vk::raii::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = *vertShaderModule,
        .pName = "main"};

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = *fragShaderModule,
        .pName = "main"};

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {
        vertShaderStageInfo,
        fragShaderStageInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology = vk::PrimitiveTopology::eTriangleList,
        .primitiveRestartEnable = vk::False};

    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount = 1,
        .scissorCount = 1};

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False};

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA};

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = vk::False,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};

    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor};

    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    vk::DescriptorSetLayout setLayouts[] = {*descriptorSetLayout};

    vk::PushConstantRange pushConstantRange{
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .offset = 0,
        .size = sizeof(ObjectPushConstants)};

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 1,
        .pSetLayouts = setLayouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange};

    pipelineLayout = vk::raii::PipelineLayout{vulkanContext.device, pipelineLayoutInfo};

    vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &vulkanContext.swapchainImageFormat};

    vk::GraphicsPipelineCreateInfo pipelineInfo{
        .pNext = &pipelineRenderingCreateInfo,
        .stageCount = static_cast<uint32_t>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = *pipelineLayout,
        .renderPass = nullptr,
        .subpass = 0};

    graphicsPipeline = vk::raii::Pipeline{
        vulkanContext.device, nullptr, pipelineInfo};
}

void cleanup()
{
    glfwDestroyWindow(vulkanContext.window);
    glfwTerminate();
}

void createCommandBuffers()
{
    commandBuffers.clear();
    
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = *vulkanContext.commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

    commandBuffers = vk::raii::CommandBuffers{vulkanContext.device, allocInfo};
}

void createSyncObjects()
{
    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();

    imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{
        .flags = vk::FenceCreateFlagBits::eSignaled};

    for (size_t i = 0; i < vulkanContext.swapchainImages.size(); i++)
    {
        renderFinishedSemaphores.emplace_back(vulkanContext.device, semaphoreInfo);
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        imageAvailableSemaphores.emplace_back(vulkanContext.device, semaphoreInfo);
        inFlightFences.emplace_back(vulkanContext.device, fenceInfo);
    }
}

void transitionImageLayout(
    vk::raii::CommandBuffer const &commandBuffer,
    vk::Image image,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    vk::PipelineStageFlags srcStage,
    vk::AccessFlags srcAccess,
    vk::PipelineStageFlags dstStage,
    vk::AccessFlags dstAccess)
{
    vk::ImageMemoryBarrier barrier{
        .srcAccessMask = srcAccess,
        .dstAccessMask = dstAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = vk::ImageSubresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1}};

    commandBuffer.pipelineBarrier(
        srcStage,
        dstStage,
        {},
        nullptr,
        nullptr,
        barrier);
}

void recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex)
{
    auto &commandBuffer = commandBuffers[frameIndex];

    commandBuffer.begin(vk::CommandBufferBeginInfo{});

    transitionImageLayout(
        commandBuffer,
        vulkanContext.swapchainImages[imageIndex],
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits::eTopOfPipe,
        {},
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eColorAttachmentWrite);

    vk::ClearValue clearColor{
        .color = vk::ClearColorValue{
            .float32 = std::array<float, 4>{0.02f, 0.02f, 0.04f, 1.0f}}};

    vk::RenderingAttachmentInfo colorAttachment{
        .imageView = *vulkanContext.swapchainImageViews[imageIndex],
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clearColor};

    vk::RenderingInfo renderingInfo{
        .renderArea = vk::Rect2D{
            .offset = vk::Offset2D{0, 0},
            .extent = vulkanContext.swapchainExtent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment};

    commandBuffer.beginRendering(renderingInfo);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);

    vk::Viewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(vulkanContext.swapchainExtent.width),
        .height = static_cast<float>(vulkanContext.swapchainExtent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f};

    vk::Rect2D scissor{
        .offset = vk::Offset2D{0, 0},
        .extent = vulkanContext.swapchainExtent};

    commandBuffer.setViewport(0, viewport);
    commandBuffer.setScissor(0, scissor);

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *pipelineLayout,
        0,
        *descriptorSets[frameIndex],
        nullptr);

    for (const Object3D &object : objects)
    {
        ObjectPushConstants pushConstants{
            .model = object.model};

        commandBuffer.pushConstants<ObjectPushConstants>(
            *pipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            std::array<ObjectPushConstants, 1>{pushConstants});

        commandBuffer.draw(object.mesh->vertexCount, 1, 0, 0);
    }

    commandBuffer.endRendering();

    transitionImageLayout(
        commandBuffer,
        vulkanContext.swapchainImages[imageIndex],
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eColorAttachmentWrite,
        vk::PipelineStageFlagBits::eBottomOfPipe,
        {});

    commandBuffer.end();
}

void updateUniformBuffer(uint32_t currentImage)
{
    CameraBufferObject camera{};

    camera.view = glm::lookAt(
        glm::vec3(2.0f, 2.0f, 2.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f));

    camera.proj = glm::perspective(glm::radians(45.0f), static_cast<float>(vulkanContext.swapchainExtent.width) / static_cast<float>(vulkanContext.swapchainExtent.height), 0.1f, 10.0f);

    camera.proj[1][1] *= -1.0f;

    memcpy(uniformBuffersMapped[currentImage], &camera, sizeof(camera));
}

void updateObjectTransforms()
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();

    float time = std::chrono::duration<float, std::chrono::seconds::period>(
                     currentTime - startTime)
                     .count();

    objects[0].model =
        glm::translate(glm::mat4(1.0f), glm::vec3(-0.75f, 0.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));

    objects[1].model =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.75f, 0.0f, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
}

void drawFrame()
{
    vulkanContext.device.waitForFences(
        *inFlightFences[currentFrame], vk::True, UINT64_MAX);

    uint32_t imageIndex = vulkanContext.swapchain.acquireNextImage(UINT64_MAX, *imageAvailableSemaphores[currentFrame], nullptr).value;

    updateUniformBuffer(currentFrame);
    updateObjectTransforms();

    vulkanContext.device.resetFences(*inFlightFences[currentFrame]);

    commandBuffers[currentFrame].reset();
    recordCommandBuffer(currentFrame, imageIndex);

    vk::Semaphore waitSemaphores[] = {
        *imageAvailableSemaphores[currentFrame]};

    vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};

    vk::Semaphore signalSemaphores[] = {
        *renderFinishedSemaphores[imageIndex]};

    vk::CommandBuffer commandBuffer = *commandBuffers[currentFrame];

    vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores};

    vulkanContext.graphicsQueue.submit(submitInfo, *inFlightFences[currentFrame]);

    vk::SwapchainKHR swapchains[] = {*vulkanContext.swapchain};

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &imageIndex};

    vulkanContext.presentQueue.presentKHR(presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties memProperties = vulkanContext.physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        bool typeMatches = typeFilter & (1 << i);
        bool hasPropertoes =
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties;

        if (typeMatches && hasPropertoes)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory");
}

BufferWithMemory createBuffer(
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    vk::MemoryPropertyFlags properties)
{
    vk::BufferCreateInfo bufferInfo{
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive};

    vk::raii::Buffer buffer = vk::raii::Buffer(vulkanContext.device, bufferInfo);

    vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(
            memRequirements.memoryTypeBits,
            properties)};

    vk::raii::DeviceMemory bufferMemory(vulkanContext.device, allocInfo);

    buffer.bindMemory(*bufferMemory, 0);

    return {
        std::move(buffer),
        std::move(bufferMemory)};
}

void createUniformBuffers()
{
    vk::DeviceSize bufferSize = sizeof(CameraBufferObject);

    uniformBuffers.clear();
    uniformBuffersMemory.clear();
    uniformBuffersMapped.clear();

    uniformBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.reserve(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.reserve(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        BufferWithMemory bufferWithMemory = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);

        uniformBuffers.emplace_back(std::move(bufferWithMemory.buffer));
        uniformBuffersMemory.emplace_back(std::move(bufferWithMemory.memory));

        uniformBuffersMapped.push_back(
            uniformBuffersMemory.back().mapMemory(0, bufferSize));
    }
}

void createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
        .pImmutableSamplers = nullptr};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = 1,
        .pBindings = &uboLayoutBinding};

    descriptorSetLayout = vk::raii::DescriptorSetLayout(vulkanContext.device, layoutInfo);
}

void createDescriptorPool()
{
    vk::DescriptorPoolSize poolSize{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)};

    vk::DescriptorPoolCreateInfo poolInfo{
        .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize};

    descriptorPool = vk::raii::DescriptorPool(vulkanContext.device, poolInfo);
}

void createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(
        MAX_FRAMES_IN_FLIGHT,
        *descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInf{
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .pSetLayouts = layouts.data()};

    descriptorSets = vk::raii::DescriptorSets(vulkanContext.device, allocInf);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = *uniformBuffers[i],
            .offset = 0,
            .range = sizeof(CameraBufferObject)};

        vk::WriteDescriptorSet descriptorWrite{
            .dstSet = *descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfo,
            .pTexelBufferView = nullptr};

        vulkanContext.device.updateDescriptorSets(descriptorWrite, nullptr);
    }
}

int main()
{
    std::cout << "Spaceguy running\n";

    setupVulkan();

    createDescriptorSetLayout();
    createGraphicsPipeline();

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    createSceneObjects();

    createCommandBuffers();
    createSyncObjects();

    while (!glfwWindowShouldClose(vulkanContext.window))
    {
        glfwPollEvents();
        drawFrame();
    }

    vulkanContext.device.waitIdle();

    cleanup();

    return 0;
}