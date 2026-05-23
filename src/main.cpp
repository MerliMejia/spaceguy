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
#include <array>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <cstring>

#include "engine/vulkanBackend.h"
#include "engine/vulkanRenderer.h"
#include "engine/predefined/vulkanGraphicPipelines.h"
#include "engine/vulkanGlobals.h"

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

void cleanup()
{
    vulkanContext.device.waitIdle();

    // since the device is in a global object, we need to manually clear?
    vulkanRendererContext.inFlightFences.clear();
    vulkanRendererContext.imageAvailableSemaphores.clear();
    vulkanRendererContext.renderFinishedSemaphores.clear();

    glfwDestroyWindow(vulkanContext.window);
    glfwTerminate();
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

int main()
{
    std::cout << "Spaceguy running\n";

    setupVulkan();

    setupRenderer();

    createSceneObjects();

    while (!glfwWindowShouldClose(vulkanContext.window))
    {
        glfwPollEvents();
        drawFrame();
    }

    cleanup();

    return 0;
}