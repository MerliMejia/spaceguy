#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

#include "engine/vulkanBackend.h"
#include "engine/vulkanRenderer.h"
#include "engine/predefined/vulkanGraphicPipelines.h"
#include "engine/vulkanGlobals.h"
#include "engine/blender/importer.h"

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

int main()
{
    std::cout << "Spaceguy running\n";

    std::cout << "Loading assets/Cube.3d...\n";

    try
    {
        _3D model = loadModel("assets/Cube.3d");

        std::cout << "Loaded object: " << model.name << "\n";
        std::cout << "Vertices: " << model.vertices.size() << "\n";
        std::cout << "Indices: " << model.indices.size() << "\n";
        std::cout << "Animations: " << model.animations.size() << "\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << "Failed to load model: " << error.what() << "\n";
    }

    setupVulkan();

    setupRenderer();

    while (!glfwWindowShouldClose(vulkanContext.window))
    {
        glfwPollEvents();
        drawFrame();
    }

    cleanup();

    return 0;
}