#include "vulkanRenderer.h"
#include "../systems/animationSystem.h"
#include "../systems/resourceManagementSystem.h"
#include "../systems/worldSystem.h"
#include "../utils/buffers.h"
#include "./predefined/vulkanDescriptorSetLayouts.h"
#include "./predefined/vulkanGraphicPipelines.h"
#include "./vulkanBackend.h"
#include <glm/gtc/constants.hpp>

VulkanRendererContext vulkanRendererContext{};

void createDescriptorSetLayout() {
  DEFAULT_DESCRIPTOR_SET_LAYOUT(vulkanRendererContext.descriptorSetLayout,
                                vulkanContext.device);
  ANIMATED_DESCRIPTOR_SET_LAYOUT(
      vulkanRendererContext.animatedDescriptorSetLayout, vulkanContext.device);
}

void createDescriptorPool() {
  DEFAULT_DESCRIPTOR_POOL(vulkanRendererContext.descriptorPool,
                          vulkanContext.device);
  ANIMATED_DESCRIPTOR_POOL(vulkanRendererContext.animatedDescriptorPool,
                           vulkanContext.device);
}

void createStaticDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(
      MAX_FRAMES_IN_FLIGHT, *vulkanRendererContext.descriptorSetLayout);

  vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = *vulkanRendererContext.descriptorPool,
      .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .pSetLayouts = layouts.data()};

  vulkanRendererContext.descriptorSets =
      vk::raii::DescriptorSets(vulkanContext.device, allocInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo cameraBufferInfo{
        .buffer = *vulkanRendererContext.uniformBuffers[i],
        .offset = 0,
        .range = sizeof(CameraBufferObject)};

    vk::WriteDescriptorSet descriptorWrite{
        .dstSet = *vulkanRendererContext.descriptorSets[i],
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .pBufferInfo = &cameraBufferInfo};

    vulkanContext.device.updateDescriptorSets(descriptorWrite, nullptr);
  }
}

void createAnimatedDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(
      MAX_FRAMES_IN_FLIGHT, *vulkanRendererContext.animatedDescriptorSetLayout);

  vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = *vulkanRendererContext.animatedDescriptorPool,
      .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .pSetLayouts = layouts.data()};

  vulkanRendererContext.animatedDescriptorSets =
      vk::raii::DescriptorSets(vulkanContext.device, allocInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo cameraBufferInfo{
        .buffer = *vulkanRendererContext.uniformBuffers[i],
        .offset = 0,
        .range = sizeof(CameraBufferObject)};

    vk::DescriptorBufferInfo animationBufferInfo{
        .buffer = *vulkanRendererContext.animationPositionsBuffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE};

    std::array<vk::WriteDescriptorSet, 2> descriptorWrites{
        vk::WriteDescriptorSet{
            .dstSet = *vulkanRendererContext.animatedDescriptorSets[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &cameraBufferInfo},
        vk::WriteDescriptorSet{
            .dstSet = *vulkanRendererContext.animatedDescriptorSets[i],
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &animationBufferInfo}};

    vulkanContext.device.updateDescriptorSets(descriptorWrites, nullptr);
  }
}

void createGraphicsPipeline() {
  DEFAULT_GRAPHICS_PIPELINE();
  ANIMATED_GRAPHICS_PIPELINE();
  DEBUG_GRAPHICS_PIPELINE();
}

void createDebugBuffers() {
  constexpr uint32_t maxDebugVertices = 65536;
  vk::DeviceSize bufferSize = sizeof(DebugVertex) * maxDebugVertices;

  vulkanRendererContext.debugFrames.clear();
  vulkanRendererContext.debugFrames.reserve(MAX_FRAMES_IN_FLIGHT);

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    BufferWithMemory bufferWithMemory =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    DebugFrameData frame{};
    frame.vertexBuffer = std::move(bufferWithMemory.buffer);
    frame.vertexMemory = std::move(bufferWithMemory.memory);
    frame.mapped = frame.vertexMemory.mapMemory(0, bufferSize);
    frame.vertexCount = 0;

    vulkanRendererContext.debugFrames.emplace_back(std::move(frame));
  }
}

void createUniformBuffers() {
  vk::DeviceSize bufferSize = sizeof(CameraBufferObject);

  vulkanRendererContext.uniformBuffers.clear();
  vulkanRendererContext.uniformBuffersMemory.clear();
  vulkanRendererContext.uniformBuffersMapped.clear();

  vulkanRendererContext.uniformBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.uniformBuffersMemory.reserve(MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.uniformBuffersMapped.reserve(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    BufferWithMemory bufferWithMemory =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    vulkanRendererContext.uniformBuffers.emplace_back(
        std::move(bufferWithMemory.buffer));
    vulkanRendererContext.uniformBuffersMemory.emplace_back(
        std::move(bufferWithMemory.memory));

    vulkanRendererContext.uniformBuffersMapped.push_back(
        vulkanRendererContext.uniformBuffersMemory.back().mapMemory(
            0, bufferSize));
  }
}

void createCommandBuffers() {
  vulkanRendererContext.commandBuffers.clear();

  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = *vulkanContext.commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

  vulkanRendererContext.commandBuffers =
      vk::raii::CommandBuffers{vulkanContext.device, allocInfo};
}

void createSyncObjects() {
  vulkanRendererContext.imageAvailableSemaphores.clear();
  vulkanRendererContext.renderFinishedSemaphores.clear();
  vulkanRendererContext.inFlightFences.clear();

  vulkanRendererContext.imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.renderFinishedSemaphores.reserve(
      vulkanContext.swapchainImages.size());
  vulkanRendererContext.inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);

  vk::SemaphoreCreateInfo semaphoreInfo{};
  vk::FenceCreateInfo fenceInfo{.flags = vk::FenceCreateFlagBits::eSignaled};

  for (size_t i = 0; i < vulkanContext.swapchainImages.size(); i++) {
    vulkanRendererContext.renderFinishedSemaphores.emplace_back(
        vulkanContext.device, semaphoreInfo);
  }

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vulkanRendererContext.imageAvailableSemaphores.emplace_back(
        vulkanContext.device, semaphoreInfo);
    vulkanRendererContext.inFlightFences.emplace_back(vulkanContext.device,
                                                      fenceInfo);
  }
}

void createDepthResources() {

  vk::ImageCreateInfo imageInfo{
      .imageType = vk::ImageType::e2D,
      .format = vulkanRendererContext.depthFormat,
      .extent = vk::Extent3D{.width = vulkanContext.swapchainExtent.width,
                             .height = vulkanContext.swapchainExtent.height,
                             .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined};

  vulkanRendererContext.depthImage =
      vk::raii::Image{vulkanContext.device, imageInfo};

  vk::MemoryRequirements memRequirements =
      vulkanRendererContext.depthImage.getMemoryRequirements();

  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          findMemoryType(memRequirements.memoryTypeBits,
                         vk::MemoryPropertyFlagBits::eDeviceLocal),
  };

  vulkanRendererContext.depthImageMemory =
      vk::raii::DeviceMemory{vulkanContext.device, allocInfo};

  vulkanRendererContext.depthImage.bindMemory(
      *vulkanRendererContext.depthImageMemory, 0);

  vk::ImageViewCreateInfo createInfo{
      .image = *vulkanRendererContext.depthImage,
      .viewType = vk::ImageViewType::e2D,
      .format = vulkanRendererContext.depthFormat,
      .subresourceRange = vk::ImageSubresourceRange{
          .aspectMask = vk::ImageAspectFlagBits::eDepth,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1}};

  vulkanRendererContext.depthImageView =
      vk::raii::ImageView{vulkanContext.device, createInfo};
}

void setupRendererCore() {
  createCommandBuffers();
  createDepthResources();
  createUniformBuffers();
  createDebugBuffers();
  createSyncObjects();
}

void setupRendererAfterAssetsLoaded() {
  createDescriptorSetLayout();
  createGraphicsPipeline();
  createDescriptorPool();
  createStaticDescriptorSets();
  if (vulkanRendererContext.animationPositionCount > 0) {
    createAnimatedDescriptorSets();
  }
}

void transitionImageLayout(vk::raii::CommandBuffer const &commandBuffer,
                           vk::Image image, vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout,
                           vk::PipelineStageFlags srcStage,
                           vk::AccessFlags srcAccess,
                           vk::PipelineStageFlags dstStage,
                           vk::AccessFlags dstAccess,
                           vk::ImageAspectFlags aspectMask) {
  vk::ImageMemoryBarrier barrier{
      .srcAccessMask = srcAccess,
      .dstAccessMask = dstAccess,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = vk::ImageSubresourceRange{.aspectMask = aspectMask,
                                                    .baseMipLevel = 0,
                                                    .levelCount = 1,
                                                    .baseArrayLayer = 0,
                                                    .layerCount = 1}};

  commandBuffer.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr,
                                barrier);
}

void recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex) {
  auto &commandBuffer = vulkanRendererContext.commandBuffers[frameIndex];

  commandBuffer.begin(vk::CommandBufferBeginInfo{});

  transitionImageLayout(
      commandBuffer, vulkanContext.swapchainImages[imageIndex],
      vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
      vk::PipelineStageFlagBits::eTopOfPipe, {},
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::AccessFlagBits::eColorAttachmentWrite,
      vk::ImageAspectFlagBits::eColor);

  vk::ClearValue clearColor{
      .color = vk::ClearColorValue{
          .float32 = std::array<float, 4>{0.02f, 0.02f, 0.04f, 1.0f}}};

  vk::RenderingAttachmentInfo colorAttachment{
      .imageView = *vulkanContext.swapchainImageViews[imageIndex],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearColor};

  vk::ClearValue clearDepth{
      .depthStencil =
          vk::ClearDepthStencilValue{
              .depth = 1.0f,
              .stencil = 0,
          },
  };

  vk::RenderingAttachmentInfo depthAttachment{
      .imageView = *vulkanRendererContext.depthImageView,
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eDontCare,
      .clearValue = clearDepth,
  };

  vk::RenderingInfo renderingInfo{
      .renderArea = vk::Rect2D{.offset = vk::Offset2D{0, 0},
                               .extent = vulkanContext.swapchainExtent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
      .pDepthAttachment = &depthAttachment};

  transitionImageLayout(commandBuffer, *vulkanRendererContext.depthImage,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eDepthAttachmentOptimal,
                        vk::PipelineStageFlagBits::eTopOfPipe, {},
                        vk::PipelineStageFlagBits::eEarlyFragmentTests,
                        vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                        vk::ImageAspectFlagBits::eDepth);

  commandBuffer.beginRendering(renderingInfo);

  vk::Viewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(vulkanContext.swapchainExtent.width),
      .height = static_cast<float>(vulkanContext.swapchainExtent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f};

  vk::Rect2D scissor{.offset = vk::Offset2D{0, 0},
                     .extent = vulkanContext.swapchainExtent};

  commandBuffer.setViewport(0, viewport);
  commandBuffer.setScissor(0, scissor);

  commandBuffer.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, *vulkanRendererContext.pipelineLayout,
      0, *vulkanRendererContext.descriptorSets[frameIndex], nullptr);

  for (const Renderable &renderable : resources.renderables) {
    if (!isEntityAlive(renderable.entity) || !renderable.visible)
      continue;

    const TransformComponent &transform = getTransform(renderable.entity);

    if (renderable.renderKind == ObjectRenderKind::Animated) {
      commandBuffer.bindPipeline(
          vk::PipelineBindPoint::eGraphics,
          *vulkanRendererContext.animatedGraphicsPipeline);

      commandBuffer.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          *vulkanRendererContext.animatedPipelineLayout, 0,
          *vulkanRendererContext.animatedDescriptorSets[frameIndex], nullptr);

      AnimationDataFromObject animationData =
          getAnimationDataFromEntity(renderable.entity);

      AnimatedObjectPushConstants pushConstants{
          .model = transform.model,
          .previousPositionOffset = animationData.previousPositionOffset,
          .nextPositionOffset = animationData.nextPositionOffset,
          .interpolation = animationData.interpolation,
          .vertexCount = renderable.animatedMesh->mesh.vertexCount,
      };

      commandBuffer.pushConstants<AnimatedObjectPushConstants>(
          *vulkanRendererContext.animatedPipelineLayout,
          vk::ShaderStageFlagBits::eVertex, 0, pushConstants);

      vk::Buffer vertexBuffers[] = {
          *renderable.animatedMesh->mesh.vertexBuffer};
      vk::DeviceSize offsets[] = {0};

      commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
      commandBuffer.bindIndexBuffer(*renderable.animatedMesh->mesh.indexBuffer,
                                    0, vk::IndexType::eUint16);

      commandBuffer.drawIndexed(renderable.animatedMesh->mesh.indexCount, 1, 0,
                                0, 0);
    } else if (renderable.renderKind == ObjectRenderKind::TransformAnimated) {
      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 *vulkanRendererContext.graphicsPipeline);

      commandBuffer.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          *vulkanRendererContext.pipelineLayout, 0,
          *vulkanRendererContext.descriptorSets[frameIndex], nullptr);

      ObjectPushConstants pushConstants{
          .model = transform.model,
      };

      commandBuffer.pushConstants<ObjectPushConstants>(
          *vulkanRendererContext.pipelineLayout,
          vk::ShaderStageFlagBits::eVertex, 0, pushConstants);

      vk::Buffer vertexBuffers[] = {
          *renderable.transformAnimatedMesh->mesh.vertexBuffer,
      };
      vk::DeviceSize offsets[] = {0};

      commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
      commandBuffer.bindIndexBuffer(
          *renderable.transformAnimatedMesh->mesh.indexBuffer, 0,
          vk::IndexType::eUint16);

      commandBuffer.drawIndexed(
          renderable.transformAnimatedMesh->mesh.indexCount, 1, 0, 0, 0);
    } else {
      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 *vulkanRendererContext.graphicsPipeline);

      commandBuffer.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          *vulkanRendererContext.pipelineLayout, 0,
          *vulkanRendererContext.descriptorSets[frameIndex], nullptr);

      ObjectPushConstants pushConstants{.model = transform.model};

      commandBuffer.pushConstants<ObjectPushConstants>(
          *vulkanRendererContext.pipelineLayout,
          vk::ShaderStageFlagBits::eVertex, 0, pushConstants);

      vk::Buffer vertexBuffers[] = {*renderable.mesh->vertexBuffer};
      vk::DeviceSize offsets[] = {0};

      commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
      commandBuffer.bindIndexBuffer(*renderable.mesh->indexBuffer, 0,
                                    vk::IndexType::eUint16);

      commandBuffer.drawIndexed(renderable.mesh->indexCount, 1, 0, 0, 0);
    }
  }

  if (vulkanRendererContext.isDebug) {
    const DebugFrameData &debugFrame =
        vulkanRendererContext.debugFrames[frameIndex];

    if (debugFrame.vertexCount > 0) {
      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 *vulkanRendererContext.debugGraphicsPipeline);

      CameraBufferObject camera{};
      camera.view =
          glm::lookAt(worldContext.cameraPosition,
                      worldContext.cameraPosition + worldContext.cameraLookAt,
                      glm::vec3{0.0f, 0.0f, 1.0f});

      camera.proj = glm::perspective(
          worldContext.cameraFovY,
          static_cast<float>(vulkanContext.swapchainExtent.width) /
              static_cast<float>(vulkanContext.swapchainExtent.height),
          worldContext.cameraClipStart, worldContext.cameraClipEnd);

      camera.proj[1][1] *= -1.0f;

      glm::mat4 viewProj = camera.proj * camera.view;

      commandBuffer.pushConstants<glm::mat4>(
          *vulkanRendererContext.debugPipelineLayout,
          vk::ShaderStageFlagBits::eVertex, 0, viewProj);

      vk::Buffer vertexBuffers[] = {*debugFrame.vertexBuffer};
      vk::DeviceSize offsets[] = {0};

      commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
      commandBuffer.draw(debugFrame.vertexCount, 1, 0, 0);
    }
  }

  commandBuffer.endRendering();

  transitionImageLayout(
      commandBuffer, vulkanContext.swapchainImages[imageIndex],
      vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::AccessFlagBits::eColorAttachmentWrite,
      vk::PipelineStageFlagBits::eBottomOfPipe, {},
      vk::ImageAspectFlagBits::eColor);

  commandBuffer.end();
}

// Need to update this
void updateUniformBuffer(uint32_t currentImage) {
  CameraBufferObject camera{};

  camera.view =
      glm::lookAt(worldContext.cameraPosition,
                  worldContext.cameraPosition + worldContext.cameraLookAt,
                  glm::vec3{0.0f, 0.0f, 1.0f});

  camera.proj = glm::perspective(
      worldContext.cameraFovY,
      static_cast<float>(vulkanContext.swapchainExtent.width) /
          static_cast<float>(vulkanContext.swapchainExtent.height),
      worldContext.cameraClipStart, worldContext.cameraClipEnd);

  camera.proj[1][1] *= -1.0f;

  memcpy(vulkanRendererContext.uniformBuffersMapped[currentImage], &camera,
         sizeof(camera));
}

void updateObjectTransforms() {
  // Not needed for now.
}

void clearDebugShapes() { vulkanRendererContext.debugVertices.clear(); }

void addDebugLine(glm::vec3 a, glm::vec3 b, glm::vec4 color) {
  vulkanRendererContext.debugVertices.push_back(DebugVertex{
      .position = a,
      .color = color,
  });

  vulkanRendererContext.debugVertices.push_back(DebugVertex{
      .position = b,
      .color = color,
  });
}

void addDebugDiskXY(glm::vec3 center, float radius, glm::vec4 color) {
  constexpr int segments = 64;

  for (int i = 0; i < segments; i++) {
    float a0 = glm::two_pi<float>() * static_cast<float>(i) /
               static_cast<float>(segments);
    float a1 = glm::two_pi<float>() * static_cast<float>(i + 1) /
               static_cast<float>(segments);

    glm::vec3 p0{
        center.x + glm::cos(a0) * radius,
        center.y + glm::sin(a0) * radius,
        center.z,
    };

    glm::vec3 p1{
        center.x + glm::cos(a1) * radius,
        center.y + glm::sin(a1) * radius,
        center.z,
    };

    addDebugLine(p0, p1, color);
  }
}

void addDebugCube(glm::vec3 center, float radius, glm::vec4 color) {
  glm::vec3 min = center - glm::vec3{radius};
  glm::vec3 max = center + glm::vec3{radius};

  glm::vec3 p000{min.x, min.y, min.z};
  glm::vec3 p001{min.x, min.y, max.z};
  glm::vec3 p010{min.x, max.y, min.z};
  glm::vec3 p011{min.x, max.y, max.z};
  glm::vec3 p100{max.x, min.y, min.z};
  glm::vec3 p101{max.x, min.y, max.z};
  glm::vec3 p110{max.x, max.y, min.z};
  glm::vec3 p111{max.x, max.y, max.z};

  addDebugLine(p000, p100, color);
  addDebugLine(p100, p110, color);
  addDebugLine(p110, p010, color);
  addDebugLine(p010, p000, color);

  addDebugLine(p001, p101, color);
  addDebugLine(p101, p111, color);
  addDebugLine(p111, p011, color);
  addDebugLine(p011, p001, color);

  addDebugLine(p000, p001, color);
  addDebugLine(p100, p101, color);
  addDebugLine(p110, p111, color);
  addDebugLine(p010, p011, color);
}

void addDebugSphere(glm::vec3 center, float radius, glm::vec4 color) {
  constexpr int segments = 32;

  for (int i = 0; i < segments; i++) {
    float a0 = glm::two_pi<float>() * static_cast<float>(i) /
               static_cast<float>(segments);
    float a1 = glm::two_pi<float>() * static_cast<float>(i + 1) /
               static_cast<float>(segments);

    float c0 = glm::cos(a0);
    float s0 = glm::sin(a0);
    float c1 = glm::cos(a1);
    float s1 = glm::sin(a1);

    // XY circle
    addDebugLine(center + glm::vec3{c0 * radius, s0 * radius, 0.0f},
                 center + glm::vec3{c1 * radius, s1 * radius, 0.0f}, color);

    // XZ circle
    addDebugLine(center + glm::vec3{c0 * radius, 0.0f, s0 * radius},
                 center + glm::vec3{c1 * radius, 0.0f, s1 * radius}, color);

    // YZ circle
    addDebugLine(center + glm::vec3{0.0f, c0 * radius, s0 * radius},
                 center + glm::vec3{0.0f, c1 * radius, s1 * radius}, color);
  }
}

void drawFrame() {
  vulkanContext.device.waitForFences(
      *vulkanRendererContext.inFlightFences[vulkanRendererContext.currentFrame],
      vk::True, UINT64_MAX);

  uint32_t imageIndex =
      vulkanContext.swapchain
          .acquireNextImage(UINT64_MAX,
                            *vulkanRendererContext.imageAvailableSemaphores
                                 [vulkanRendererContext.currentFrame],
                            nullptr)
          .value;

  updateUniformBuffer(vulkanRendererContext.currentFrame);
  updateObjectTransforms();

  if (vulkanRendererContext.isDebug) {
    DebugFrameData &debugFrame =
        vulkanRendererContext.debugFrames[vulkanRendererContext.currentFrame];

    debugFrame.vertexCount =
        static_cast<uint32_t>(vulkanRendererContext.debugVertices.size());

    if (debugFrame.vertexCount > 0) {
      memcpy(debugFrame.mapped, vulkanRendererContext.debugVertices.data(),
             sizeof(DebugVertex) * debugFrame.vertexCount);
    }
  } else {
    vulkanRendererContext.debugFrames[vulkanRendererContext.currentFrame]
        .vertexCount = 0;
  }

  vulkanContext.device.resetFences(
      *vulkanRendererContext
           .inFlightFences[vulkanRendererContext.currentFrame]);

  vulkanRendererContext.commandBuffers[vulkanRendererContext.currentFrame]
      .reset();
  recordCommandBuffer(vulkanRendererContext.currentFrame, imageIndex);

  vk::Semaphore waitSemaphores[] = {
      *vulkanRendererContext
           .imageAvailableSemaphores[vulkanRendererContext.currentFrame]};

  vk::PipelineStageFlags waitStages[] = {
      vk::PipelineStageFlagBits::eColorAttachmentOutput};

  vk::Semaphore signalSemaphores[] = {
      *vulkanRendererContext.renderFinishedSemaphores[imageIndex]};

  vk::CommandBuffer commandBuffer =
      *vulkanRendererContext.commandBuffers[vulkanRendererContext.currentFrame];

  vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
                            .pWaitSemaphores = waitSemaphores,
                            .pWaitDstStageMask = waitStages,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &commandBuffer,
                            .signalSemaphoreCount = 1,
                            .pSignalSemaphores = signalSemaphores};

  vulkanContext.graphicsQueue.submit(
      submitInfo, *vulkanRendererContext
                       .inFlightFences[vulkanRendererContext.currentFrame]);

  vk::SwapchainKHR swapchains[] = {*vulkanContext.swapchain};

  vk::PresentInfoKHR presentInfo{.waitSemaphoreCount = 1,
                                 .pWaitSemaphores = signalSemaphores,
                                 .swapchainCount = 1,
                                 .pSwapchains = swapchains,
                                 .pImageIndices = &imageIndex};

  vulkanContext.presentQueue.presentKHR(presentInfo);

  vulkanRendererContext.currentFrame =
      (vulkanRendererContext.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void uploadAnimationPositions(const std::vector<glm::vec4> &positions) {
  vk::DeviceSize bufferSize = sizeof(glm::vec4) * positions.size();

  BufferWithMemory buffer =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eStorageBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void *data = buffer.memory.mapMemory(0, bufferSize);
  memcpy(data, positions.data(), static_cast<size_t>(bufferSize));
  buffer.memory.unmapMemory();

  vulkanRendererContext.animationPositionsBuffer = std::move(buffer.buffer);
  vulkanRendererContext.animationPositionsMemory = std::move(buffer.memory);
  vulkanRendererContext.animationPositionCount =
      static_cast<uint32_t>(positions.size());
}
