#include "engine/renderer/imports.h" // IWYU pragma: keep
#include "engine/renderer/renderGraph.h"
#include "engine/renderer/renderNode.h"
#include "engine/renderer/shaders.h"
#include "engine/renderer/vSwapChain.h"
#include <cstdlib>
#include <iostream>
#include <utility>

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

  Renderer::RenderNode *testNode1 = nullptr;

  void initVulkan() {
    vInstance.create();
    window.createSurface(vInstance.handler);
    vDevice.pickAndCreate(vInstance.handler, window.surface);
    vSwapChain.create(vDevice.physicalDevice, window.surface, vDevice.device,
                      window.handler);

    testNode1 = &renderGraph.createNode();

    testNode1->updateUniforms = true;
    testNode1->usePushConstants = true;

    glm::mat4 model = rotate(glm::mat4(1.0f), glm::radians(90.0f),
                             glm::vec3(0.0f, 0.0f, 1.0f));

    const uint32_t modelIndex = 5;

    Renderer::Shaders::UniformBank::setFloat4x4(
        renderGraph.context.globalUniformBufferData.data, modelIndex, model);

    Renderer::Shaders::PushConstantsBank::setUInt(
        renderGraph.context.pushConstantBank, 3, modelIndex);

    testNode1->step1_initShaders(
        vDevice.device,
        Renderer::step1_initShadersProps{
            .shaderCreateInfos = {Renderer::ShaderCreateInfo{
                                      .type = Renderer::ShaderType::Vertex,
                                      .name = "vertMain"},
                                  Renderer::ShaderCreateInfo{
                                      .type = Renderer::ShaderType::Fragment,
                                      .name = "fragMain"}},
            .shaderFile = "shaders/v2/testNode1.spv"});

    // step 1.1: define vertex input the same as in the shader:
    const std::vector<Renderer::DefaultVertex> vertices{
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

    const std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

    testNode1->step_1_1_createAndFillVertexBuffer<Renderer::DefaultVertex>(
        vertices, vDevice);

    testNode1->step_1_2_createAndFillIndicesBuffer(indices, vDevice);

    testNode1->step_1_3_createUniformBuffers(vDevice);

    testNode1->step_1_4_createDescriptorSetLayout(vDevice.device);

    testNode1->step_1_5_createDescriptorPool(vDevice.device);

    testNode1->step_1_6_allocateDescriptorSets(vDevice.device);

    testNode1->step_1_7_configureDescriptorSets(vDevice.device);

    testNode1->step2_initPipelineConfiguration<Renderer::DefaultVertex>(
        vDevice.device, vSwapChain.swapChainSurfaceFormat,
        Renderer::step2_pipelineConfigurationProps{});

    testNode1->step3_initCommandBuffer(vDevice.queueIndex, vDevice.device);

    renderGraph.init(vDevice.device, vSwapChain.swapChainImages);
  }

  void mainLoop() {

    int loopCounts = 0;

    window.update([this, &loopCounts]() {
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
          glm::radians(45.0f),
          static_cast<float>(800) / static_cast<float>(600), 0.1f, 10.0f);

      proj[1][1] *= -1;

      Renderer::Shaders::UniformBank::setFloat4x4(
          renderGraph.context.globalUniformBufferData.data, 5, model);
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
