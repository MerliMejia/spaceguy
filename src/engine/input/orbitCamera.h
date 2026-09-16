#pragma once

#include "glm/fwd.hpp"
#include <GLFW/glfw3.h>

#include "../renderer/shaders/shaders.h"

namespace Input {
namespace Camera {
struct OrbitCamera {
  glm::vec2 lastMouse{0.0f};
  glm::vec2 deltaMouse{0.0f};
  bool hasLastMouse = false;
  glm::vec3 up{0.0f, 0.0f, 1.0f};
  glm::vec3 position;
  glm::vec3 direction;
  glm::vec3 target;
  float fovY = 0;
  float clipStart = 0;
  float clipEnd = 0;
  float distance = 5.0f;
  float yaw = 0.0f;
  float pitch = 0.0f;
  float orbitSen = 0.005f;
  float zoomSen = 0.005f;

  glm::mat4 view;
  glm::mat4 proj;

  void init(GLFWwindow *window);

  void update(uint32_t width, uint32_t height,
              Renderer::Shaders::UniformBank::Data &uniformsBank,
              GLFWwindow *window);
};
} // namespace Camera
} // namespace Input
