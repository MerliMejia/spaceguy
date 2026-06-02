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
#include "utils/time.h"
#include "systems/animationSystem.h"

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

    setupRendererCore();

    std::cout << "Spaceguy running\n";

    std::cout << "Loading assets/Wizzard_4.3d...\n";
    AnimatedMesh animatedMesh;
    std::vector<glm::vec4> animationPositions;

    BlenderModel model = loadModel("assets/Wizzard_4.3d");

    std::cout << "Loaded object: " << model.name << "\n";
    std::cout << "Vertices: " << model.vertices.size() << "\n";
    std::cout << "Indices: " << model.indices.size() << "\n";
    std::cout << "Animations: " << model.animations.size() << "\n";

    for (const AnimationClip &clip : model.animations)
    {
        for (const AnimationKeyPose &keyPoses : clip.keyPoses)
        {
            for (const glm::vec3 &pos : keyPoses.positions)
            {
                animationPositions.push_back(glm::vec4(pos, 1.0f));
            }
        }
    }

    uploadAnimationPositions(animationPositions);

    animatedMesh = generateAnimatedMesh(model, 0);

    std::cout << "Mesh: indices: "
              << animatedMesh.mesh.indexCount
              << " vertices: "
              << animatedMesh.mesh.vertexCount
              << std::endl;

    setupRendererAfterAssetsLoaded();

    vulkanRendererContext.objects.push_back(Object3D{
        .renderKind = ObjectRenderKind::Animated,
        .animatedMesh = &animatedMesh,
        .model = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f)),
        .activeAnimation = 0,
        .activeFrame = 0});

    while (!glfwWindowShouldClose(vulkanContext.window))
    {
        glfwPollEvents();
        updateTime();
        updateAnimations();
        drawFrame();
    }

    cleanup();

    return 0;
}