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
#include "utils/generators.h"

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
    setupVulkan();

    setupRenderer();

    std::cout << "Spaceguy running\n";

    std::cout << "Loading assets/Cube.3d...\n";
    Mesh modelMesh;

    try
    {
        BlenderModel model = loadModel("assets/Cube.3d");

        std::cout << "Loaded object: " << model.name << "\n";
        std::cout << "Vertices: " << model.vertices.size() << "\n";
        std::cout << "Indices: " << model.indices.size() << "\n";
        std::cout << "Animations: " << model.animations.size() << "\n";

        modelMesh = generateMesh(model.vertices, model.indices);
    }
    catch (const std::exception &error)
    {
        std::cerr << "Failed to load model: " << error.what() << "\n";
    }

    std::cout << "Mesh: " << "indices: " << modelMesh.indexCount << " vertices: " << modelMesh.vertexCount << std::endl;

    vulkanRendererContext.objects.push_back(Object3D{
        .mesh = &modelMesh,
        .model = glm::translate(glm::mat4(1.0f), glm::vec3(-0.75f, 0.0f, 0.0f)) *
                 glm::scale(glm::mat4(1.0f), glm::vec3(0.5f))});

    vulkanRendererContext.objects.push_back(Object3D{
        .mesh = &modelMesh,
        .model =
            glm::translate(glm::mat4(1.0f), glm::vec3(0.75f, 0.0f, 0.0f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(0.5f))});

    while (!glfwWindowShouldClose(vulkanContext.window))
    {
        glfwPollEvents();
        drawFrame();
    }

    cleanup();

    return 0;
}