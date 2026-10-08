#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <random>
#include <vector>

struct Transform {
  glm::vec3 position;
  glm::vec3 scale;
  glm::quat rotation;
};

glm::mat4 transformToModel(glm::vec3 position, glm::quat rotation,
                           glm::vec3 scale);
Transform modelToTransform(const glm::mat4 &model);
glm::vec2 randomPointInCircle(float centerX, float centerY, float radius);
bool isCloseBox(glm::vec2 p1, glm::vec2 p2, float threshold);
void moveTowardsDir(glm::mat4 &model, float speed, glm::vec2 dir,
                    float distance, float deltaTime);
float getDistanceSqr(glm::vec2 p1, glm::vec2 p2);
void faceTowardsDir(glm::mat4 &model, glm::vec2 dir);

inline int randomInt(int size) {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_int_distribution<int> dist(0, size);

  return dist(gen);
}

inline float randomFloat(int size) {
  static std::random_device rd;
  static std::mt19937 gen(rd());

  std::uniform_real_distribution<float> dist(1, size);

  return dist(gen);
}

std::vector<glm::vec3> generatePointsInSphere(int n, double radius,
                                              glm::vec3 center);

template <typename T>
size_t randomIndexFromValue(const std::vector<T> &vector) {
  static std::mt19937 rng(std::random_device{}());

  std::uniform_int_distribution<size_t> dist(0, vector.size() - 1);
  return dist(rng);
}

inline constexpr float CHECK_RADIUS = 4.0f;
inline constexpr float RADIUS_SQ = CHECK_RADIUS * CHECK_RADIUS;
inline constexpr float SUPER_CLOSE_RADIUS = 1.1f;
inline constexpr float SUPER_CLOSE_RADIUS_SQ =
    SUPER_CLOSE_RADIUS * SUPER_CLOSE_RADIUS;
