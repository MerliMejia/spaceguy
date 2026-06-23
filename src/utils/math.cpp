#include "math.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/trigonometric.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <random>

glm::mat4 transformToModel(glm::vec3 position, glm::quat rotation,
                           glm::vec3 scale) {
  glm::mat4 translation = glm::translate(glm::mat4{1.0f}, position);
  glm::mat4 rot = glm::mat4_cast(glm::normalize(rotation));
  glm::mat4 sca = glm::scale(glm::mat4{1.0f}, scale);

  return translation * rot * sca;
}

glm::vec2 randomPointInCircle(float centerX, float centerY, float radius) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(-radius, radius);

  float xOffset, yOffset;

  // Rejection loop
  do {
    xOffset = dist(gen);
    yOffset = dist(gen);
  } while ((xOffset * xOffset) + (yOffset * yOffset) > (radius * radius));

  return glm::vec2{centerX + xOffset, centerY + yOffset};
}

bool isCloseBox(glm::vec2 p1, glm::vec2 p2, float threshold) {
  if (std::abs(p1.x - p2.x) > threshold)
    return false;
  if (std::abs(p1.y - p2.y) > threshold)
    return false;
  return true;
}

float getDistanceSqr(glm::vec2 p1, glm::vec2 p2) {
  glm::vec2 delta = p2 - p1;
  return glm::dot(delta, delta);
}

void moveTowardsDir(glm::mat4 &model, float speed, glm::vec2 dir,
                    float distance, float deltaTime) {

  if (distance < 0.001f)
    return;

  Transform transform = modelToTransform(model);

  float angleRadians = glm::atan(dir.y, dir.x) + glm::half_pi<float>();

  float step = speed * deltaTime;
  if (step > distance)
    step = distance;

  transform.position += glm::vec3(dir * step, 0.0f);
  transform.rotation = glm::angleAxis(angleRadians, glm::vec3{0.0, 0.0, 1.0});

  model =
      transformToModel(transform.position, transform.rotation, transform.scale);
}

void faceTowardsDir(glm::mat4 &model, glm::vec2 dir) {
  Transform transform = modelToTransform(model);
  float angleRadians = glm::atan(dir.y, dir.x) + glm::half_pi<float>();

  transform.rotation = glm::angleAxis(angleRadians, glm::vec3{0.0, 0.0, 1.0});

  model =
      transformToModel(transform.position, transform.rotation, transform.scale);
}

int getRandom(int size) {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_int_distribution<int> dist(0, size - 1);

  return dist(gen);
}

Transform modelToTransform(const glm::mat4 &model) {
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
