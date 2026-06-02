#include "input.h"
#include "../engine/vulkanRenderer.h"
#include "../engine/vulkanBackend.h"

bool keyPressedOnce(int key)
{
    static bool keyWasDown[GLFW_KEY_LAST + 1]{};

    const bool isDown = glfwGetKey(vulkanContext.window, key) == GLFW_PRESS;
    const bool pressed = isDown && !keyWasDown[key];

    keyWasDown[key] = isDown;

    return pressed;
}

void handleAnimationInput()
{
    if (vulkanRendererContext.objects.empty())
    {
        return;
    }
}
