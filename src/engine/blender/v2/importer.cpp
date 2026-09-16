#include "importer.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Blender::V2 {

namespace {

using Tokens = std::vector<std::string>;

Tokens readTokensIgnoringComments(std::istream &input) {
  Tokens tokens;
  std::string line;

  while (std::getline(input, line)) {
    std::istringstream lineInput(line);
    lineInput >> std::ws;

    if (lineInput.eof() || lineInput.peek() == '#') {
      continue;
    }

    while (true) {
      lineInput >> std::ws;

      if (lineInput.eof()) {
        break;
      }

      std::string token;

      if (!(lineInput >> std::quoted(token))) {
        throw std::runtime_error("Malformed quoted token");
      }

      tokens.push_back(std::move(token));
    }
  }

  if (input.bad()) {
    throw std::runtime_error("Failed while reading model file");
  }

  return tokens;
}

const std::string &next(const Tokens &tokens, std::size_t &cursor) {
  if (cursor >= tokens.size()) {
    throw std::runtime_error("Unexpected end of file");
  }

  return tokens[cursor++];
}

void expect(const Tokens &tokens, std::size_t &cursor,
            const std::string &expected) {
  const std::string &actual = next(tokens, cursor);

  if (actual != expected) {
    throw std::runtime_error("Expected '" + expected + "', got '" + actual +
                             "'");
  }
}

std::string readString(const Tokens &tokens, std::size_t &cursor) {
  return next(tokens, cursor);
}

int readInt(const Tokens &tokens, std::size_t &cursor) {
  const std::string &token = next(tokens, cursor);
  std::size_t used = 0;
  const int value = std::stoi(token, &used);

  if (used != token.size()) {
    throw std::runtime_error("Invalid integer: " + token);
  }

  return value;
}

std::uint32_t readUInt32(const Tokens &tokens, std::size_t &cursor) {
  const std::string &token = next(tokens, cursor);

  if (token.empty() ||
      token.find_first_not_of("0123456789") != std::string::npos) {
    throw std::runtime_error("Invalid unsigned integer: " + token);
  }

  const unsigned long long value = std::stoull(token);

  if (value > std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("Unsigned integer exceeds uint32 range");
  }

  return static_cast<std::uint32_t>(value);
}

std::size_t readSize(const Tokens &tokens, std::size_t &cursor) {
  const std::size_t value = readUInt32(tokens, cursor);

  // Every counted element requires at least one remaining token.
  if (value > tokens.size() - cursor) {
    throw std::runtime_error("Count exceeds remaining file data");
  }

  return value;
}

float readFloat(const Tokens &tokens, std::size_t &cursor) {
  const std::string &token = next(tokens, cursor);
  std::size_t used = 0;
  const float value = std::stof(token, &used);

  if (used != token.size() || !std::isfinite(value)) {
    throw std::runtime_error("Invalid finite float: " + token);
  }

  return value;
}

bool readBool(const Tokens &tokens, std::size_t &cursor) {
  const std::string &token = next(tokens, cursor);

  if (token == "true") {
    return true;
  }

  if (token == "false") {
    return false;
  }

  throw std::runtime_error("Invalid boolean: " + token);
}

glm::vec2 readVec2(const Tokens &tokens, std::size_t &cursor) {
  glm::vec2 value{};
  value.x = readFloat(tokens, cursor);
  value.y = readFloat(tokens, cursor);
  return value;
}

glm::vec3 readVec3(const Tokens &tokens, std::size_t &cursor) {
  glm::vec3 value{};
  value.x = readFloat(tokens, cursor);
  value.y = readFloat(tokens, cursor);
  value.z = readFloat(tokens, cursor);
  return value;
}

glm::quat readQuat(const Tokens &tokens, std::size_t &cursor) {
  glm::quat value{};
  value.w = readFloat(tokens, cursor);
  value.x = readFloat(tokens, cursor);
  value.y = readFloat(tokens, cursor);
  value.z = readFloat(tokens, cursor);

  const double lengthSquared = static_cast<double>(value.w) * value.w +
                               static_cast<double>(value.x) * value.x +
                               static_cast<double>(value.y) * value.y +
                               static_cast<double>(value.z) * value.z;

  if (lengthSquared <= 0.0) {
    throw std::runtime_error("Rotation quaternion cannot be zero");
  }

  return value;
}

void requireValues(const Tokens &tokens, std::size_t cursor, std::size_t count,
                   std::size_t valuesPerElement,
                   const std::string &description) {
  if (count > (tokens.size() - cursor) / valuesPerElement) {
    throw std::runtime_error("Truncated " + description);
  }
}

AnimationKind readAnimationKind(const Tokens &tokens, std::size_t &cursor) {
  const std::string kind = readString(tokens, cursor);

  if (kind == "vertex") {
    return AnimationKind::Vertex;
  }

  if (kind == "transform") {
    return AnimationKind::Transform;
  }

  throw std::runtime_error("Unsupported animation type: " + kind);
}

void readClipHeader(const Tokens &tokens, std::size_t &cursor,
                    AnimationClip &clip) {
  expect(tokens, cursor, "loop");
  clip.loop = readBool(tokens, cursor);

  expect(tokens, cursor, "start_frame");
  clip.startFrame = readInt(tokens, cursor);

  expect(tokens, cursor, "end_frame");
  clip.endFrame = readInt(tokens, cursor);

  if (clip.startFrame > clip.endFrame) {
    throw std::runtime_error("Animation '" + clip.name +
                             "' has an invalid frame range");
  }
}

// Shared by transform poses and attachment poses.
// Both types expose blenderFrame, location, rotation, and scale.
template <typename Pose>
Pose readTransformPose(const Tokens &tokens, std::size_t &cursor) {
  Pose pose{};

  expect(tokens, cursor, "key_pose");
  pose.blenderFrame = readInt(tokens, cursor);

  expect(tokens, cursor, "location");
  pose.location = readVec3(tokens, cursor);

  expect(tokens, cursor, "rotation_quaternion");
  pose.rotation = readQuat(tokens, cursor);

  expect(tokens, cursor, "scale");
  pose.scale = readVec3(tokens, cursor);

  return pose;
}

template <typename Pose>
void validatePoseFrames(const std::vector<Pose> &poses,
                        const AnimationClip &clip) {
  for (std::size_t i = 0; i < poses.size(); ++i) {
    const int frame = poses[i].blenderFrame;

    if (frame < clip.startFrame || frame > clip.endFrame) {
      throw std::runtime_error("Animation '" + clip.name +
                               "' has a pose outside its frame range");
    }

    if (i > 0 && frame <= poses[i - 1].blenderFrame) {
      throw std::runtime_error("Animation '" + clip.name +
                               "' pose frames must be strictly increasing");
    }
  }
}

void readVertexKeyPoses(const Tokens &tokens, std::size_t &cursor,
                        AnimationClip &clip, std::size_t vertexCount,
                        std::size_t poseCount) {
  // Each pose has at least: key_pose <frame>.
  requireValues(tokens, cursor, poseCount, 2, "vertex animation");

  for (std::size_t i = 0; i < poseCount; ++i) {
    AnimationKeyPose pose{};

    expect(tokens, cursor, "key_pose");
    pose.blenderFrame = readInt(tokens, cursor);

    requireValues(tokens, cursor, vertexCount, 6, "vertex animation pose");

    pose.positions.resize(vertexCount);
    pose.normals.resize(vertexCount);

    for (std::size_t vertexIndex = 0; vertexIndex < vertexCount;
         ++vertexIndex) {
      pose.positions[vertexIndex] = readVec3(tokens, cursor);
      pose.normals[vertexIndex] = readVec3(tokens, cursor);
    }

    clip.keyPoses.push_back(std::move(pose));
  }

  validatePoseFrames(clip.keyPoses, clip);
}

void readTransformKeyPoses(const Tokens &tokens, std::size_t &cursor,
                           AnimationClip &clip, std::size_t poseCount) {
  // key_pose + frame + three labels + ten transform values.
  requireValues(tokens, cursor, poseCount, 15, "transform animation");

  for (std::size_t i = 0; i < poseCount; ++i) {
    clip.transformKeyPoses.push_back(
        readTransformPose<TransformAnimationKeyPose>(tokens, cursor));
  }

  validatePoseFrames(clip.transformKeyPoses, clip);
}

void readAttachments(const Tokens &tokens, std::size_t &cursor,
                     AnimationClip &clip) {
  expect(tokens, cursor, "attachment_count");
  const std::size_t attachmentCount = readSize(tokens, cursor);

  // attachment <name> bone <name> key_pose_count <count>
  requireValues(tokens, cursor, attachmentCount, 6, "attachments");

  for (std::size_t i = 0; i < attachmentCount; ++i) {
    AnimationAttachment attachment{};

    expect(tokens, cursor, "attachment");
    attachment.objectName = readString(tokens, cursor);

    expect(tokens, cursor, "bone");
    attachment.boneName = readString(tokens, cursor);

    expect(tokens, cursor, "key_pose_count");
    const std::size_t poseCount = readSize(tokens, cursor);

    if (poseCount == 0) {
      throw std::runtime_error("Attachment must contain at least one pose");
    }

    requireValues(tokens, cursor, poseCount, 15, "attachment poses");

    for (std::size_t poseIndex = 0; poseIndex < poseCount; ++poseIndex) {
      attachment.keyPoses.push_back(
          readTransformPose<AttachmentAnimationKeyPose>(tokens, cursor));
    }

    validatePoseFrames(attachment.keyPoses, clip);
    clip.attachments.push_back(std::move(attachment));
  }
}

BlenderModel readModel(const Tokens &tokens, std::size_t &cursor,
                       const std::filesystem::path &modelPath) {
  expect(tokens, cursor, "spaceguy_3d");

  if (readInt(tokens, cursor) != 7) {
    throw std::runtime_error("The v2 importer requires spaceguy_3d 7");
  }

  BlenderModel model{};

  expect(tokens, cursor, "object_name");
  model.name = readString(tokens, cursor);

  expect(tokens, cursor, "texture_path");

  const auto textureReference =
      std::filesystem::u8path(readString(tokens, cursor));

  if (textureReference.empty() || textureReference.has_root_path()) {
    throw std::runtime_error("Expected a relative texture path");
  }

  model.texturePath =
      (modelPath.parent_path() / textureReference).lexically_normal();

  expect(tokens, cursor, "fps");
  model.fps = readFloat(tokens, cursor);

  if (model.fps <= 0.0f) {
    throw std::runtime_error("FPS must be positive");
  }

  expect(tokens, cursor, "vertex_count");
  const std::size_t vertexCount = readSize(tokens, cursor);

  expect(tokens, cursor, "index_count");
  const std::size_t indexCount = readSize(tokens, cursor);

  expect(tokens, cursor, "animation_count");
  const std::size_t animationCount = readSize(tokens, cursor);

  if (indexCount % 3 != 0) {
    throw std::runtime_error("Index count must be divisible by three");
  }

  expect(tokens, cursor, "vertices");

  requireValues(tokens, cursor, vertexCount, 8, "base vertices");

  model.vertices.resize(vertexCount);

  for (Vertex &vertex : model.vertices) {
    vertex.pos = readVec3(tokens, cursor);
    vertex.uv = readVec2(tokens, cursor);
    vertex.normal = readVec3(tokens, cursor);
  }

  expect(tokens, cursor, "indices");

  requireValues(tokens, cursor, indexCount, 1, "indices");

  model.indices.resize(indexCount);

  for (std::uint32_t &index : model.indices) {
    index = readUInt32(tokens, cursor);

    if (index >= vertexCount) {
      throw std::runtime_error("Vertex index is out of range");
    }
  }

  expect(tokens, cursor, "animations");

  for (std::size_t i = 0; i < animationCount; ++i) {
    AnimationClip clip{};

    expect(tokens, cursor, "animation");
    clip.name = readString(tokens, cursor);

    expect(tokens, cursor, "type");
    clip.kind = readAnimationKind(tokens, cursor);

    readClipHeader(tokens, cursor, clip);

    expect(tokens, cursor, "key_pose_count");
    const std::size_t poseCount = readSize(tokens, cursor);

    if (poseCount == 0) {
      throw std::runtime_error("Animation '" + clip.name +
                               "' must contain at least one pose");
    }

    if (clip.kind == AnimationKind::Vertex) {
      readVertexKeyPoses(tokens, cursor, clip, vertexCount, poseCount);

      readAttachments(tokens, cursor, clip);
    } else {
      readTransformKeyPoses(tokens, cursor, clip, poseCount);
    }

    model.animations.push_back(std::move(clip));
  }

  if (cursor != tokens.size()) {
    throw std::runtime_error("Unexpected data after model");
  }

  return model;
}

} // namespace

BlenderModel loadModel(const std::string &path) {
  try {
    const auto modelPath =
        std::filesystem::absolute(std::filesystem::u8path(path))
            .lexically_normal();

    std::ifstream file(modelPath);

    if (!file) {
      throw std::runtime_error("Could not open model file");
    }

    const Tokens tokens = readTokensIgnoringComments(file);
    std::size_t cursor = 0;

    return readModel(tokens, cursor, modelPath);

  } catch (const std::exception &error) {
    throw std::runtime_error(path + ": " + error.what());
  }
}

BlenderTransformModel loadTransformModel(const std::string &path) {
  BlenderModel source = loadModel(path);

  BlenderTransformModel model{};
  model.name = std::move(source.name);
  model.texturePath = std::move(source.texturePath);
  model.fps = source.fps;
  model.vertices = std::move(source.vertices);
  model.indices = std::move(source.indices);

  for (AnimationClip &sourceClip : source.animations) {
    if (sourceClip.kind != AnimationKind::Transform) {
      continue;
    }

    TransformAnimationClip clip{};
    clip.name = std::move(sourceClip.name);
    clip.startFrame = sourceClip.startFrame;
    clip.endFrame = sourceClip.endFrame;
    clip.keyPoses = std::move(sourceClip.transformKeyPoses);
    clip.loop = sourceClip.loop;

    model.animations.push_back(std::move(clip));
  }

  return model;
}

static ImporterTransform readTransform(const Tokens &tokens,
                                       std::size_t &cursor) {
  ImporterTransform transform{};

  expect(tokens, cursor, "position");
  transform.position = readVec3(tokens, cursor);

  expect(tokens, cursor, "rotation");
  transform.rotation = readVec3(tokens, cursor);

  expect(tokens, cursor, "scale");
  transform.scale = readVec3(tokens, cursor);

  return transform;
}

WorldData loadWorldData() {
  std::ifstream file("assets/world.world");

  if (!file) {
    throw std::runtime_error("Could not open file: assets/world.world");
  }

  const Tokens tokens = readTokensIgnoringComments(file);
  std::size_t cursor = 0;

  WorldData data{};

  expect(tokens, cursor, "spaceguy_world");

  const int version = readInt(tokens, cursor);

  if (version != 2) {
    throw std::runtime_error("Unsupported .world version");
  }

  expect(tokens, cursor, "floor");
  data.floor = readTransform(tokens, cursor);

  expect(tokens, cursor, "camera");
  data.camera.transform = readTransform(tokens, cursor);

  expect(tokens, cursor, "look_direction");
  data.camera.direction = readVec3(tokens, cursor);

  expect(tokens, cursor, "fov_y");
  data.camera.fovY = readFloat(tokens, cursor);

  expect(tokens, cursor, "clip_start");
  data.camera.clipStart = readFloat(tokens, cursor);

  expect(tokens, cursor, "clip_end");
  data.camera.clipEnd = readFloat(tokens, cursor);

  expect(tokens, cursor, "wizards");

  expect(tokens, cursor, "wizard_count");
  data.wizards.count = readInt(tokens, cursor);

  data.wizards.positions.resize(static_cast<std::size_t>(data.wizards.count));

  for (glm::vec3 &position : data.wizards.positions) {
    position = readVec3(tokens, cursor);
  }

  expect(tokens, cursor, "ogres");

  expect(tokens, cursor, "ogre_count");
  data.ogres.count = readInt(tokens, cursor);

  data.ogres.positions.resize(static_cast<std::size_t>(data.ogres.count));

  for (glm::vec3 &position : data.ogres.positions) {
    position = readVec3(tokens, cursor);
  }

  return data;
}

} // namespace Blender::V2
