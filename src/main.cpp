#include "engine/renderer/imports.h" // IWYU pragma: keep
#include "engine/renderer/renderGraph.h"
#include "engine/renderer/renderNode.h"
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

  Renderer::RenderNode triangleNode{};
  Renderer::RenderGraph renderGraph{};

  void initVulkan() {
    vInstance.create();
    window.createSurface(vInstance.handler);
    vDevice.pickAndCreate(vInstance.handler, window.surface);
    vSwapChain.create(vDevice.physicalDevice, window.surface, vDevice.device,
                      window.handler);

    triangleNode.step1_initShaders(
        vDevice.device,
        Renderer::step1_initShadersProps{
            .shaderCreateInfos = {Renderer::ShaderCreateInfo{
                                      .type = Renderer::ShaderType::Vertex,
                                      .name = "vertMain"},
                                  Renderer::ShaderCreateInfo{
                                      .type = Renderer::ShaderType::Fragment,
                                      .name = "fragMain"}},
            .shaderFile = "shaders/v2/default.spv"});

    // step 1.1: define vertex input the same as in the shader:
    const std::vector<Renderer::DefaultVertex> vertices{
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

    const std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

    triangleNode.step_1_1_createAndFillVertexBuffer<Renderer::DefaultVertex>(
        vertices, vDevice);

    triangleNode.step_1_2_createAndFillIndicesBuffer(indices, vDevice);

    triangleNode.step_1_3_createUniformBuffers(vDevice);

    triangleNode.step_1_4_createDescriptorSetLayout(vDevice.device);

    triangleNode.step_1_5_createDescriptorPool(vDevice.device);

    triangleNode.step_1_6_allocateDescriptorSets(vDevice.device);

    triangleNode.step_1_7_configureDescriptorSets(vDevice.device);

    triangleNode.step2_initPipelineConfiguration<Renderer::DefaultVertex>(
        vDevice.device, vSwapChain.swapChainSurfaceFormat,
        Renderer::step2_pipelineConfigurationProps{});

    triangleNode.step3_initCommandBuffer(vDevice.queueIndex, vDevice.device);

    renderGraph.renderNodes.push_back(std::move(triangleNode));

    renderGraph.init(vDevice.device, vSwapChain.swapChainImages);
  }

  void mainLoop() {
    window.update([this]() {
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
