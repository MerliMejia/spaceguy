#include "orbitCamera.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace Input {
namespace Camera {

static void cursor_position_callback(GLFWwindow *window, double xpos,
                                     double ypos) {
  auto *camera = static_cast<OrbitCamera *>(glfwGetWindowUserPointer(window));

  if (!camera) {
    return;
  }

  const glm::vec2 currentMouse{
      static_cast<float>(xpos),
      static_cast<float>(ypos),
  };

  if (camera->hasLastMouse) {
    camera->deltaMouse += currentMouse - camera->lastMouse;
  }

  camera->lastMouse = currentMouse;
  camera->hasLastMouse = true;
}

void OrbitCamera::init(GLFWwindow *window) {
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, cursor_position_callback);

  glfwSetWindowUserPointer(window, this);

  target = glm::vec3{0.0f, 0.0f, 0.0f};

  const glm::vec3 offset = position - target;
  distance = glm::length(offset);
  direction = glm::normalize(target - position);
}

void OrbitCamera::update(uint32_t width, uint32_t height,
                         Renderer::Shaders::UniformBank::Data &uniformsBank,
                         GLFWwindow *window) {

  glm::vec3 offset = position - target;

  // Mouse X: rotate the camera's offset around world X.
  const float angle = deltaMouse.x * orbitSen;

  const glm::mat4 rotation =
      glm::rotate(glm::mat4{1.0f}, angle, glm::vec3{0.0f, 0.0f, 1.0f});

  offset = glm::vec3(rotation * glm::vec4(offset, 0.0f));
  up = glm::normalize(glm::vec3(rotation * glm::vec4(up, 0.0f)));

  if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)) {
    // Mouse Y: move closer to or farther from the target.
    distance = glm::length(offset);
    distance *= std::exp(deltaMouse.y * zoomSen);
    distance = std::clamp(distance, 20.0f, 50.0f);
  }

  position = target + glm::normalize(offset) * distance;

  direction = glm::normalize(target - position);

  view = glm::lookAt(position, target, glm::vec3{0.0f, 0.0f, 1.0f});

  proj = glm::perspective(
      fovY, static_cast<float>(width) / static_cast<float>(height), clipStart,
      clipEnd);

  proj[1][1] *= -1.0f;

  Renderer::Shaders::UniformBank::setFloat4x4(uniformsBank, SG_VIEW_INDEX,
                                              view);
  Renderer::Shaders::UniformBank::setFloat4x4(uniformsBank, SG_PROJ_INDEX,
                                              proj);
  Renderer::Shaders::UniformBank::setFloat4(uniformsBank, SG_VIEW_POS_INDEX,
                                            glm::vec4(position, 1.0f));

  deltaMouse = glm::vec2{0.0f};
}
} // namespace Camera
} // namespace Input
