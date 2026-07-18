#include "lightsSystem.h"

#include "../engine/vulkanRenderer.h"
#include "resourceManagementSystem.h"
#include <algorithm>

static glm::vec3
getPointLightWorldPosition(const PointLightComponent &pointLight) {
  if (const TransformComponent *transform =
          tryGetTransform(pointLight.entity)) {
    return glm::vec3(transform->model[3]);
  }

  return pointLight.position;
}

void writeLightsToSceneBuffer(SceneBufferObject &scene) {
  scene.sunDirection = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
  scene.sunColorIntensity = glm::vec4(0.0f);
  scene.lightCounts = glm::uvec4(0u);

  if (!resources.sunLights.empty()) {
    const SunLightComponent &sun = resources.sunLights.front();
    scene.sunDirection = glm::vec4(glm::normalize(sun.direction), 0.0f);
    scene.sunColorIntensity = glm::vec4(sun.color, sun.intensity);
  }

  const uint32_t pointLightCount = std::min(
      static_cast<uint32_t>(resources.pointLights.size()), MAX_POINT_LIGHTS);
  scene.lightCounts.x = pointLightCount;

  for (uint32_t i = 0; i < pointLightCount; ++i) {
    const PointLightComponent &pointLight = resources.pointLights[i];
    scene.pointLights[i] = PointLightGpu{
        .position =
            glm::vec4(getPointLightWorldPosition(pointLight), 1.0f),
        .colorIntensity =
            glm::vec4(pointLight.color, pointLight.intensity),
        .attenuation = glm::vec4(pointLight.attenuation, 0.0f),
    };
  }
}

void updateLightsSystem() {
  if (!vulkanRendererContext.isDebug) {
    return;
  }

  constexpr float debugCubeRadius = 0.25f;
  for (const PointLightComponent &pointLight : resources.pointLights) {
    if (!isEntityAlive(pointLight.entity)) {
      continue;
    }

    addDebugCube(getPointLightWorldPosition(pointLight), debugCubeRadius,
                 glm::vec4(pointLight.color, 1.0f));
  }
}
