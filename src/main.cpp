#include "engine/renderer/imports.h" // IWYU pragma: keep
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
  Renderer::Window window;
  Renderer::VInstance vInstance;
  Renderer::VPhysicalDevice vPhysicalDevice;

  void initVulkan() {
    vInstance.create();
    vPhysicalDevice.pick(vInstance.handler);
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
