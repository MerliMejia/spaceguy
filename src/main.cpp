#include "engine/renderer/imports.h" // IWYU pragma: keep
#include "engine/renderer/renderNode.h"
#include "engine/renderer/vSwapChain.h"
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

  Renderer::RenderNode triangleNode;

  void initVulkan() {
    vInstance.create();
    window.createSurface(vInstance.handler);
    vDevice.pickAndCreate(vInstance.handler, window.surface);
    vSwapChain.create(vDevice.physicalDevice, window.surface, vDevice.device,
                      window.handler);

    triangleNode.step1_initShaders(
        vDevice.device,
        Renderer::step1_initShadersProps{
            .shaderCreateInfos = {
                Renderer::ShaderCreateInfo{.type = Renderer::ShaderType::Vertex,
                                           .name = "vertMain"},
                Renderer::ShaderCreateInfo{.type =
                                               Renderer::ShaderType::Fragment,
                                           .name = "fragMain"}}});

    triangleNode.step2_pipelineConfiguration(
        vDevice.device, vSwapChain.swapChainSurfaceFormat,
        Renderer::step2_pipelineConfigurationProps{});
  }

  void mainLoop() {
    window.update([]() {

    });
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
