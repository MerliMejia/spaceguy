#include "math.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/trigonometric.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <random>

glm::mat4 transformToModel(glm::vec3 position, glm::quat rotation, glm::vec3 scale) {
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

std::vector<glm::vec2> generateCirclePoints(glm::vec2 center, float radius, int numPoints) {
  std::vector<glm::vec2> points;
  points.reserve(numPoints);

  // Total radians in a full circle (2 * pi)
  float totalRadians = glm::two_pi<float>();

  for (int i = 0; i < numPoints; ++i) {
    // Calculate the current angle (fraction of the full circle)
    float theta = totalRadians * (static_cast<float>(i) / numPoints);

    // Apply parametric equations: x = h + r*cos(θ), y = k + r*sin(θ)
    glm::vec2 point;
    point.x = center.x + radius * glm::cos(theta);
    point.y = center.y + radius * glm::sin(theta);

    points.push_back(point);
  }

  return points;
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

void moveTowardsDir(glm::mat4 &model, float speed, glm::vec2 dir, float distance, float deltaTime) {

  if (distance < 0.001f)
    return;

  Transform transform = modelToTransform(model);

  float angleRadians = glm::atan(dir.y, dir.x) + glm::half_pi<float>();

  float step = speed * deltaTime;
  if (step > distance)
    step = distance;

  transform.position += glm::vec3(dir * step, 0.0f);
  transform.rotation = glm::angleAxis(angleRadians, glm::vec3{0.0, 0.0, 1.0});

  model = transformToModel(transform.position, transform.rotation, transform.scale);
}

void faceTowardsDir(glm::mat4 &model, glm::vec2 dir) {
  Transform transform = modelToTransform(model);
  float angleRadians = glm::atan(dir.y, dir.x) + glm::half_pi<float>();

  transform.rotation = glm::angleAxis(angleRadians, glm::vec3{0.0, 0.0, 1.0});

  model = transformToModel(transform.position, transform.rotation, transform.scale);
}

std::vector<glm::vec3> generatePointsOnSphere(int n, float radius, glm::vec3 center) {
  std::vector<glm::vec3> points;
  points.reserve(n);

  std::mt19937 gen(std::random_device{}());
  std::normal_distribution<float> gauss(0.0f, 1.0f); // mean 0, std-dev 1

  while (points.size() < static_cast<size_t>(n)) {
    // 1. Random vector: same probability in every direction
    glm::vec3 v{gauss(gen), gauss(gen), gauss(gen)};

    // 2. Skip the (extremely rare) near-zero vector to avoid dividing by ~0
    float lenSq = glm::dot(v, v);
    if (lenSq < 1e-12f) {
      continue;
    }

    // 3. Normalize -> uniform direction on the unit sphere
    glm::vec3 dir = v / std::sqrt(lenSq);

    // 4. Scale to the radius and move to the center
    points.push_back(center + dir * radius);
  }
  return points;
}

void rotateTransform(Transform &t, float angularSpeed, glm::vec3 axis, float dt) {

  float radianSpeed = glm::radians(angularSpeed);

  glm::quat deltaRotation = glm::angleAxis(angularSpeed * dt, axis);

  t.rotation = deltaRotation * t.rotation;
  t.rotation = glm::normalize(t.rotation);
}

Transform modelToTransform(const glm::mat4 &model) {
  glm::vec3 position = glm::vec3(model[3]);
  glm::vec3 scale = glm::vec3{glm::length(glm::vec3(model[0])), glm::length(glm::vec3(model[1])), glm::length(glm::vec3(model[2]))};
  glm::mat3 rotationMatrix{
      glm::vec3(model[0]) / scale.x,
      glm::vec3(model[1]) / scale.y,
      glm::vec3(model[2]) / scale.z,
  };
  glm::quat rotation = glm::quat_cast(rotationMatrix);

  return Transform{.position = position, .scale = scale, .rotation = rotation};
}

// Fits an orthographic shadow box around a bounding sphere.
// Only the direction from sunPosition to center is used, the sun's distance
// doesn't matter.
ViewProjMatrices computeSunMatrices(glm::vec3 sunPosition, glm::vec3 center, float radius, float margin) {
  glm::vec3 sunDir = glm::normalize(center - sunPosition);

  // Virtual eye just outside the sphere, on the sun's side.
  glm::vec3 eye = center - sunDir * (radius + margin);

  // Z-up world; fall back to Y-up if the sun is (almost) straight overhead.
  glm::vec3 up{0.0f, 0.0f, 1.0f};
  if (glm::abs(glm::dot(sunDir, up)) > 0.99f) {
    up = glm::vec3{0.0f, 1.0f, 0.0f};
  }

  ViewProjMatrices m;
  m.view = glm::lookAt(eye, center, up);

  // Explicit right-handed, 0..1 depth: doesn't depend on
  // GLM_FORCE_DEPTH_ZERO_TO_ONE.
  float zNear = 0.0f;
  float zFar = 2.0f * (radius + margin);
  m.proj = glm::orthoRH_ZO(-radius, radius, -radius, radius, zNear, zFar);

  // Same Vulkan Y-flip as the camera.
  m.proj[1][1] *= -1.0f;

  return m;
}
