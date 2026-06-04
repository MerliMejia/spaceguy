#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

glm::mat4 transformToModel(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale);
glm::vec3 randomPointInCircleXY(const glm::vec3 &center, float radius);