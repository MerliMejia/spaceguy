#include "vulkanRenderer.h"
#include "./predefined/vulkanDescriptorSetLayouts.h"
#include "./predefined/vulkanGraphicPipelines.h"
#include "./vulkanBackend.h"

VulkanRendererContext vulkanRendererContext{};
std::vector<Object3D> objects;
Mesh triangleMesh{
    .vertexCount = 3};

// For actual per vertex stuff:
std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, -0.5f}}, // 0
    {{0.5f, -0.5f, -0.5f}},  // 1
    {{0.5f, 0.5f, -0.5f}},   // 2
    {{-0.5f, 0.5f, -0.5f}},  // 3

    {{-0.5f, -0.5f, 0.5f}}, // 4
    {{0.5f, -0.5f, 0.5f}},  // 5
    {{0.5f, 0.5f, 0.5f}},   // 6
    {{-0.5f, 0.5f, 0.5f}},  // 7
};

std::vector<uint16_t> indices = {
    // Back face
    0,
    2,
    1,
    0,
    3,
    2,

    // Front face
    4,
    5,
    6,
    4,
    6,
    7,

    // Left face
    0,
    4,
    7,
    0,
    7,
    3,

    // Right face
    1,
    2,
    6,
    1,
    6,
    5,

    // Bottom face
    0,
    1,
    5,
    0,
    5,
    4,

    // Top face
    3,
    7,
    6,
    3,
    6,
    2,
};

struct BufferWithMemory
{
    vk::raii::Buffer buffer;
    vk::raii::DeviceMemory memory;
};

void createDescriptorSetLayout()
{
    CREATE_SIMPLE_DESCRIPTOR_SET_LAYOUT(vulkanRendererContext.descriptorSetLayout, vulkanContext.device);
}

void createDescriptorPool()
{
    CREATE_SIMPLE_DESCRIPTOR_POOL(vulkanRendererContext.descriptorPool, vulkanContext.device);
}

void createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(
        MAX_FRAMES_IN_FLIGHT,
        *vulkanRendererContext.descriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInf{
        .descriptorPool = *vulkanRendererContext.descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
        .pSetLayouts = layouts.data()};

    vulkanRendererContext.descriptorSets = vk::raii::DescriptorSets(vulkanContext.device, allocInf);

    // The way we set up descriptor sets here should change based on how we render stuff.
    // I just don't know a good way to abstract this yet.
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vk::DescriptorBufferInfo bufferInfo{
            .buffer = *vulkanRendererContext.uniformBuffers[i],
            .offset = 0,
            .range = sizeof(CameraBufferObject)};

        vk::WriteDescriptorSet descriptorWrite{
            .dstSet = *vulkanRendererContext.descriptorSets[i],
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

void createGraphicsPipeline()
{
    CREATE_SIMPLE_CAMERA_MODEL_GP();
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

    vulkanRendererContext.uniformBuffers.clear();
    vulkanRendererContext.uniformBuffersMemory.clear();
    vulkanRendererContext.uniformBuffersMapped.clear();

    vulkanRendererContext.uniformBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
    vulkanRendererContext.uniformBuffersMemory.reserve(MAX_FRAMES_IN_FLIGHT);
    vulkanRendererContext.uniformBuffersMapped.reserve(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        BufferWithMemory bufferWithMemory = createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eUniformBuffer,
            vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent);

        vulkanRendererContext.uniformBuffers.emplace_back(std::move(bufferWithMemory.buffer));
        vulkanRendererContext.uniformBuffersMemory.emplace_back(std::move(bufferWithMemory.memory));

        vulkanRendererContext.uniformBuffersMapped.push_back(
            vulkanRendererContext.uniformBuffersMemory.back().mapMemory(0, bufferSize));
    }
}

void createCommandBuffers()
{
    vulkanRendererContext.commandBuffers.clear();

    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = *vulkanContext.commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

    vulkanRendererContext.commandBuffers = vk::raii::CommandBuffers{vulkanContext.device, allocInfo};
}

void createSyncObjects()
{
    vulkanRendererContext.imageAvailableSemaphores.clear();
    vulkanRendererContext.renderFinishedSemaphores.clear();
    vulkanRendererContext.inFlightFences.clear();

    vulkanRendererContext.imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
    vulkanRendererContext.renderFinishedSemaphores.reserve(vulkanContext.swapchainImages.size());
    vulkanRendererContext.inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{
        .flags = vk::FenceCreateFlagBits::eSignaled};

    for (size_t i = 0; i < vulkanContext.swapchainImages.size(); i++)
    {
        vulkanRendererContext.renderFinishedSemaphores.emplace_back(vulkanContext.device, semaphoreInfo);
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vulkanRendererContext.imageAvailableSemaphores.emplace_back(vulkanContext.device, semaphoreInfo);
        vulkanRendererContext.inFlightFences.emplace_back(vulkanContext.device, fenceInfo);
    }
}

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

void createVertexBuffers()
{
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    auto vertexBuffer = createBuffer(bufferSize,
                                     vk::BufferUsageFlagBits::eVertexBuffer,
                                     vk::MemoryPropertyFlagBits::eHostVisible |
                                         vk::MemoryPropertyFlagBits::eHostCoherent);

    void *data = vertexBuffer.memory.mapMemory(0, bufferSize);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vertexBuffer.memory.unmapMemory();

    triangleMesh.vertexBuffer = std::move(vertexBuffer.buffer);
    triangleMesh.vertexDeviceMemory = std::move(vertexBuffer.memory);
    triangleMesh.vertexCount = static_cast<uint32_t>(vertices.size());
}

void createIndexBuffers()
{
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    auto indexBuffer = createBuffer(bufferSize,
                                    vk::BufferUsageFlagBits::eIndexBuffer,
                                    vk::MemoryPropertyFlagBits::eHostVisible |
                                        vk::MemoryPropertyFlagBits::eHostCoherent);

    void *data = indexBuffer.memory.mapMemory(0, bufferSize);
    memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
    indexBuffer.memory.unmapMemory();

    triangleMesh.indexBuffer = std::move(indexBuffer.buffer);
    triangleMesh.indexDeviceMemory = std::move(indexBuffer.memory);
    triangleMesh.indexCount = static_cast<uint32_t>(indices.size());
}

void setupRenderer()
{
    createDescriptorSetLayout();
    createGraphicsPipeline();

    createCommandBuffers();

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    createSyncObjects();

    createSceneObjects();

    // After we create scene objects since each object should have the same vertex buffer for now.
    createVertexBuffers();
    createIndexBuffers();
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
    auto &commandBuffer = vulkanRendererContext.commandBuffers[frameIndex];

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

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *vulkanRendererContext.graphicsPipeline);

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
        *vulkanRendererContext.pipelineLayout,
        0,
        *vulkanRendererContext.descriptorSets[frameIndex],
        nullptr);

    for (const Object3D &object : objects)
    {
        ObjectPushConstants pushConstants{
            .model = object.model};

        commandBuffer.pushConstants<ObjectPushConstants>(
            *vulkanRendererContext.pipelineLayout,
            vk::ShaderStageFlagBits::eVertex,
            0,
            std::array<ObjectPushConstants, 1>{pushConstants});

        // Per vertex stuff:
        vk::Buffer vertexBuffer[] = {*object.mesh->vertexBuffer};
        vk::DeviceSize offsets[] = {0};

        commandBuffer.bindVertexBuffers(0, vertexBuffer, offsets);
        commandBuffer.bindIndexBuffer(*object.mesh->indexBuffer, 0, vk::IndexType::eUint16);
        commandBuffer.drawIndexed(object.mesh->indexCount, 1, 0, 0, 0);
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

    memcpy(vulkanRendererContext.uniformBuffersMapped[currentImage], &camera, sizeof(camera));
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
        *vulkanRendererContext.inFlightFences[vulkanRendererContext.currentFrame], vk::True, UINT64_MAX);

    uint32_t imageIndex = vulkanContext.swapchain.acquireNextImage(UINT64_MAX, *vulkanRendererContext.imageAvailableSemaphores[vulkanRendererContext.currentFrame], nullptr).value;

    updateUniformBuffer(vulkanRendererContext.currentFrame);
    updateObjectTransforms();

    vulkanContext.device.resetFences(*vulkanRendererContext.inFlightFences[vulkanRendererContext.currentFrame]);

    vulkanRendererContext.commandBuffers[vulkanRendererContext.currentFrame].reset();
    recordCommandBuffer(vulkanRendererContext.currentFrame, imageIndex);

    vk::Semaphore waitSemaphores[] = {
        *vulkanRendererContext.imageAvailableSemaphores[vulkanRendererContext.currentFrame]};

    vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};

    vk::Semaphore signalSemaphores[] = {
        *vulkanRendererContext.renderFinishedSemaphores[imageIndex]};

    vk::CommandBuffer commandBuffer = *vulkanRendererContext.commandBuffers[vulkanRendererContext.currentFrame];

    vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores};

    vulkanContext.graphicsQueue.submit(submitInfo, *vulkanRendererContext.inFlightFences[vulkanRendererContext.currentFrame]);

    vk::SwapchainKHR swapchains[] = {*vulkanContext.swapchain};

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &imageIndex};

    vulkanContext.presentQueue.presentKHR(presentInfo);

    vulkanRendererContext.currentFrame = (vulkanRendererContext.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
