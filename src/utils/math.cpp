#include "math.h"
#include <random>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

glm::mat4 transformToModel(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale)
{
    glm::mat4 translation = glm::translate(glm::mat4{1.0f}, position);
    glm::quat q = glm::quat(rotation);
    glm::mat4 rot = glm::mat4_cast(q);
    glm::mat4 sca = glm::scale(glm::mat4{1.0f}, scale);

    return translation * rot * sca;
}

glm::vec3 randomPointInCircleXY(const glm::vec3 &center, float radius)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    float angle = dist(gen) * 2.0f * glm::pi<float>();
    float r = radius * std::sqrt(dist(gen));

    return center + glm::vec3{
                        std::cos(angle) * r,
                        std::sin(angle) * r,
                        0.0f};
}