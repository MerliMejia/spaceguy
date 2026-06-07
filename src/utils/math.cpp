#include "math.h"
#include "glm/fwd.hpp"
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <random>

glm::mat4 transformToModel(glm::vec3 position, glm::vec3 rotation,
                           glm::vec3 scale) {
  glm::mat4 translation = glm::translate(glm::mat4{1.0f}, position);
  glm::quat q = glm::quat(rotation);
  glm::mat4 rot = glm::mat4_cast(q);
  glm::mat4 sca = glm::scale(glm::mat4{1.0f}, scale);

  return translation * rot * sca;
}

glm::vec3 randomPointInCircleXY(const glm::vec3 &center, float radius) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  float angle = dist(gen) * 2.0f * glm::pi<float>();
  float r = radius * std::sqrt(dist(gen));

  return center + glm::vec3{std::cos(angle) * r, std::sin(angle) * r, 0.0f};
}

Transform modelToTransform(glm::mat4 &model) {
  glm::vec3 position = glm::vec3(model[3]);
  glm::vec3 scale = glm::vec3{glm::length(glm::vec3(model[0])),
                              glm::length(glm::vec3(model[1])),
                              glm::length(glm::vec3(model[2]))};
  glm::mat3 rotationMatrix{
      glm::vec3(model[0]) / scale.x,
      glm::vec3(model[1]) / scale.y,
      glm::vec3(model[2]) / scale.z,
  };
  glm::quat rotation = glm::quat_cast(rotationMatrix);

  return Transform{.position = position, .scale = scale, .rotation = rotation};
}
