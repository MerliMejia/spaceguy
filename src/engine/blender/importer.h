#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <istream>
#include "../../utils/types.h"

struct AnimationKeyPose
{
    int blenderFrame = 0;
    std::vector<glm::vec3> positions;
};

struct AnimationClip
{
    std::string name;
    int startFrame = 0;
    int endFrame = 0;
    std::vector<AnimationKeyPose> keyPoses;
};

struct BlenderModel
{
    std::string name;
    float fps = 24.0f;
    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;
    std::vector<AnimationClip> animations;
};

BlenderModel loadModel(const std::string &path);