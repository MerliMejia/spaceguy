#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../../utils/types.h"
#include <cstdint>
#include <fstream>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct AnimationKeyPose {
  int blenderFrame = 0;
  std::vector<glm::vec3> positions;
};

struct AnimationClip {
  std::string name;
  int startFrame = 0;
  int endFrame = 0;
  std::vector<AnimationKeyPose> keyPoses;
  bool loop = false;
};

struct BlenderModel {
  std::string name;
  float fps = 24.0f;
  std::vector<Vertex> vertices;
  std::vector<std::uint16_t> indices;
  std::vector<AnimationClip> animations;
};

struct ImporterTransform {
  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 scale;
};

struct Camera {
  ImporterTransform transform;
  glm::vec3 direction;
  float fovY = glm::radians(45.0f);
  float clipStart = 0.1f;
  float clipEnd = 100.0f;
};

struct Wizards {
  int count;
  std::vector<glm::vec3> positions;
};

struct WorldData {
  ImporterTransform floor;
  Camera camera;
  Wizards wizards;
};

BlenderModel loadModel(const std::string &path);
WorldData loadWorldData();
