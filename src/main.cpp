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
#include "utils/input.h"
#include "systems/worldSystem.h"

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

void setAnimation(Object3D &object, uint32_t animationIndex)
{
    if (object.animatedMesh == nullptr ||
        animationIndex >= object.animatedMesh->animations.size())
    {
        return;
    }

    object.activeAnimation = animationIndex;
    object.animationTimeSeconds = 0.0f;
    object.activeFrame =
        static_cast<uint32_t>(object.animatedMesh->animations[animationIndex].startFrame);
}

void handleAnimationChangeTest(Object3D &object)
{
    if (keyPressedOnce(GLFW_KEY_0))
    {
        setAnimation(object, 0);
    }
    else if (keyPressedOnce(GLFW_KEY_1))
    {
        setAnimation(object, 1);
    }
    else if (keyPressedOnce(GLFW_KEY_2))
    {
        setAnimation(object, 2);
    }
    else if (keyPressedOnce(GLFW_KEY_3))
    {
        setAnimation(object, 3);
    }
}

int main()
{
    setupVulkan();

    setupRendererCore();

    std::cout << "Spaceguy running\n";

    std::cout << "Loading world data...\n";
    auto worldData = loadWorldData();

    worldContext.cameraPosition = worldData.camera.transform.position;
    worldContext.cameraLookAt = worldData.camera.direction;

    BlenderModel floorModel = loadModel("assets/floor.3d");
    Mesh floorMesh = generateMesh(floorModel.vertices, floorModel.indices);

    glm::vec3 rotation = glm::radians(worldData.floor.rotation);

    glm::mat4 model{1.0f};
    model = glm::translate(model, worldData.floor.position);
    model = glm::rotate(model, rotation.x, glm::vec3{1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, rotation.y, glm::vec3{0.0f, 1.0f, 0.0f});
    model = glm::rotate(model, rotation.z, glm::vec3{0.0f, 0.0f, 1.0f});
    model = glm::scale(model, worldData.floor.scale);

    vulkanRendererContext.objects.push_back(Object3D{
        .mesh = &floorMesh,
        .renderKind = ObjectRenderKind::Static,
        .model = model});

    std::vector<glm::vec4> animationPositions;

    BlenderModel wizardModel = loadModel("assets/Wizzard_4.3d");

    for (const AnimationClip &clip : wizardModel.animations)
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

    setupRendererAfterAssetsLoaded();

    AnimatedMesh animatedMesh = generateAnimatedMesh(wizardModel, 0);

    for (const glm::vec3 &wizardPosition : worldData.wizards.positions)
    {
        glm::mat4 wizardModelMatrix{1.0f};
        wizardModelMatrix = glm::translate(wizardModelMatrix, wizardPosition);

        vulkanRendererContext.objects.push_back(Object3D{
            .renderKind = ObjectRenderKind::Animated,
            .mesh = nullptr,
            .animatedMesh = &animatedMesh,
            .model = wizardModelMatrix,
            .activeAnimation = 0,
            .activeFrame = 0,
        });
    }

    while (!glfwWindowShouldClose(vulkanContext.window))
    {
        glfwPollEvents();
        updateTime();
        handleAnimationChangeTest(vulkanRendererContext.objects[1]);
        updateAnimations();
        drawFrame();
    }

    cleanup();

    return 0;
}