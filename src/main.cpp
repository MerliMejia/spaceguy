#include "engine/blender/importer.h"
#include "engine/renderer/images/vImageManager.h"
#include "engine/renderer/renderGraph.h"
#include "engine/renderer/renderNode/colorRenderNode.h"
#include "engine/renderer/renderNode/renderNode.h"
#include "engine/renderer/shaders.h"
#include "engine/renderer/vInstance.h"
#include "engine/renderer/vSwapChain.h"
#include "engine/renderer/window.h"
#include "glm/fwd.hpp"
#include "utils/types.h"
#include <cstdlib>
#include <iostream>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

class HelloTriangleApplication {
public:
  void run() {
    window.init(WIDTH, HEIGHT, "Renderer");
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  Renderer::VInstance vInstance;
  Renderer::Window window;
  Renderer::VDevice vDevice;
  Renderer::VSwapChain vSwapChain;
  Renderer::RenderGraph::Fucntions renderGraph{};

  Renderer::ColorRenderNode colorRenderNode;
  Renderer::Images::VManager vTextureManager{};
  glm::vec3 modelCenter;

  void initVulkan() {
    vInstance.create();
    window.createSurface(vInstance.handler);
    vDevice.pickAndCreate(vInstance.handler, window.surface);
    vSwapChain.create(vDevice.physicalDevice, window.surface, vDevice.device,
                      window.handler);

    colorRenderNode.renderNode = &renderGraph.createNode();
    colorRenderNode.present = true;

    BlenderModel testModel = loadModel("assets/Ogre.3d");

    if (testModel.vertices.empty()) {
      throw std::runtime_error("Loaded model has no vertices");
    }

    glm::vec3 minPos = testModel.vertices.front().pos;
    glm::vec3 maxPos = minPos;

    for (const Vertex &vertex : testModel.vertices) {
      minPos = glm::min(minPos, vertex.pos);
      maxPos = glm::max(maxPos, vertex.pos);
    }

    modelCenter = (minPos + maxPos) * 0.5f;

    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),
                                     glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 model = glm::translate(rotation, -modelCenter);

    const uint32_t modelIndex = 0;

    Renderer::Shaders::UniformBank::setFloat4x4(
        renderGraph.context.globalUniformBufferData.data, modelIndex, model);

    Renderer::Shaders::PushConstantsBank::setUInt(
        renderGraph.context.pushConstantBank, 0, modelIndex);

    colorRenderNode.init(
        vDevice,
        Renderer::step1_initShadersProps{
            .shaderCreateInfos =
                {Renderer::RenderNodeUtils::ShaderCreateInfo{
                     .type = Renderer::RenderNodeUtils::ShaderType::Vertex,
                     .name = "vertMain"},
                 Renderer::RenderNodeUtils::ShaderCreateInfo{
                     .type = Renderer::RenderNodeUtils::ShaderType::Fragment,
                     .name = "fragMain"}},
            .shaderFile = "shaders/v2/testNode1.spv"},
        vSwapChain);

    colorRenderNode.setData<Vertex>(testModel.vertices, testModel.indices,
                                    vDevice, renderGraph.commandPool);

    vTextureManager.init(vDevice, renderGraph.commandPool);

    // Renderer::Images::VTexture *testTexture = vTextureManager.createTexture(
    //     "assets/texture.jpg", vDevice, renderGraph.commandPool,
    //     vDevice.graphicsQueue);
    // Renderer::Images::VTexture *testTexture2 = vTextureManager.createTexture(
    //     "assets/texture2.jpg", vDevice, renderGraph.commandPool,
    //     vDevice.graphicsQueue);

    // Renderer::Shaders::PushConstantsBank::setUInt(
    //     renderGraph.context.pushConstantBank, 1, testTexture->index);
    // Renderer::Shaders::PushConstantsBank::setUInt(
    //     renderGraph.context.pushConstantBank, 2, testTexture2->index);

    colorRenderNode.finish<Vertex>(vDevice, vTextureManager, vSwapChain,
                                   renderGraph.commandPool);

    renderGraph.init(vDevice.device, vSwapChain.swapChainImages);
  }

  void mainLoop() {

    int loopCounts = 0;

    window.update([this, &loopCounts]() {
      static auto startTime = std::chrono::high_resolution_clock::now();

      auto thisCurrentTime = std::chrono::high_resolution_clock::now();
      float time =
          std::chrono::duration<float>(thisCurrentTime - startTime).count();

      glm::mat4 rotation =
          glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f),
                      glm::vec3(0.0f, 0.0f, 1.0f));

      glm::mat4 model = glm::translate(rotation, -modelCenter);

      glm::mat4 view =
          lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                 glm::vec3(0.0f, 0.0f, 1.0f));

      glm::mat4 proj = glm::perspective(
          glm::radians(45.0f),
          static_cast<float>(800) / static_cast<float>(600), 0.1f, 10.0f);

      proj[1][1] *= -1;

      Renderer::Shaders::UniformBank::setFloat4x4(
          renderGraph.context.globalUniformBufferData.data, 0, model);
      Renderer::Shaders::UniformBank::setFloat4x4(
          renderGraph.context.globalUniformBufferData.data,
          Renderer::Shaders::UniformBank::viewIndex, view);
      Renderer::Shaders::UniformBank::setFloat4x4(
          renderGraph.context.globalUniformBufferData.data,
          Renderer::Shaders::UniformBank::projIndex, proj);

      renderGraph.prepareNodes(vDevice.device, vSwapChain);
      renderGraph.submit(vDevice.graphicsQueue, vSwapChain);
    });

    vDevice.device.waitIdle();
  }

  void cleanup() { window.cleanup(); }
};

int main() {
  try {
    HelloTriangleApplication app;
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
