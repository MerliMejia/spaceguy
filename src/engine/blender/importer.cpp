#include "importer.h"
#include <cmath>

static std::vector<std::string> currentTokens;
static std::size_t cursor = 0;

static std::vector<std::string>
readTokensIgnoringComments(std::istream &input) {
  std::vector<std::string> tokens;
  std::string line;

  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    std::istringstream lineInput(line);

    std::string token;
    while (lineInput >> token) {
      tokens.push_back(token);
    }
  }

  return tokens;
}

static std::string next() {
  if (cursor >= currentTokens.size()) {
    throw std::runtime_error("Unexpected end of file");
  }

  return currentTokens[cursor++];
}

static void expect(const std::string &expected) {
  const std::string actual = next();

  if (actual != expected) {
    throw std::runtime_error("Expected '" + expected + "'" + ", got '" +
                             actual + "'");
  }
}

static int readInt() { return std::stoi(next()); }

static std::size_t readSize() {
  return static_cast<std::size_t>(std::stoul(next()));
}

static std::uint32_t readUInt32() {
  return static_cast<std::uint32_t>(std::stoul(next()));
}

static float readFloat() { return std::stof(next()); }

static std::string readString() { return next(); }

static glm::vec3 readVec3() {
  glm::vec3 value{};
  value.x = readFloat();
  value.y = readFloat();
  value.z = readFloat();
  return value;
}

static glm::quat readQuat() {
  glm::quat value{};
  value.w = readFloat();
  value.x = readFloat();
  value.y = readFloat();
  value.z = readFloat();
  return value;
}

static glm::quat quatFromEulerXYZ(const glm::vec3 &rotation) {
  glm::mat4 matrix{1.0f};
  matrix = glm::rotate(matrix, rotation.x, glm::vec3{1.0f, 0.0f, 0.0f});
  matrix = glm::rotate(matrix, rotation.y, glm::vec3{0.0f, 1.0f, 0.0f});
  matrix = glm::rotate(matrix, rotation.z, glm::vec3{0.0f, 0.0f, 1.0f});
  return glm::quat_cast(matrix);
}

static AnimationKind readAnimationKind() {
  const std::string kind = readString();

  if (kind == "vertex") {
    return AnimationKind::Vertex;
  }

  if (kind == "transform") {
    return AnimationKind::Transform;
  }

  throw std::runtime_error("Unsupported animation type: " + kind);
}

static void readClipHeader(AnimationClip &clip) {
  expect("loop");
  clip.loop = readString() == "true";

  expect("start_frame");
  clip.startFrame = readInt();

  expect("end_frame");
  clip.endFrame = readInt();

  expect("key_pose_count");
}

static void readVertexKeyPoses(AnimationClip &clip, std::size_t vertexCount,
                               std::size_t frameCount,
                               bool hasAnimatedNormals) {
  clip.keyPoses.resize(frameCount);

  for (AnimationKeyPose &keyPose : clip.keyPoses) {
    expect("key_pose");
    keyPose.blenderFrame = readInt();

    keyPose.positions.resize(vertexCount);

    if (hasAnimatedNormals) {
      keyPose.normals.resize(vertexCount);
    }

    for (std::size_t vertexIndex = 0; vertexIndex < vertexCount;
         ++vertexIndex) {
      keyPose.positions[vertexIndex] = readVec3();

      if (hasAnimatedNormals) {
        keyPose.normals[vertexIndex] = readVec3();
      }
    }
  }
}

static AttachmentAnimationKeyPose readAttachmentKeyPose() {
  AttachmentAnimationKeyPose keyPose{};
  expect("key_pose");
  keyPose.blenderFrame = readInt();
  expect("location");
  keyPose.location = readVec3();
  expect("rotation_quaternion");
  keyPose.rotation = readQuat();
  expect("scale");
  keyPose.scale = readVec3();
  return keyPose;
}

static void readAttachments(AnimationClip &clip) {
  expect("attachment_count");
  const std::size_t attachmentCount = readSize();
  clip.attachments.resize(attachmentCount);

  for (AnimationAttachment &attachment : clip.attachments) {
    expect("attachment");
    attachment.objectName = readString();
    expect("bone");
    attachment.boneName = readString();
    expect("key_pose_count");
    const std::size_t keyPoseCount = readSize();
    attachment.keyPoses.reserve(keyPoseCount);

    for (std::size_t i = 0; i < keyPoseCount; ++i) {
      attachment.keyPoses.push_back(readAttachmentKeyPose());
    }
  }
}

static void readTransformKeyPoses(AnimationClip &clip, std::size_t frameCount) {
  clip.transformKeyPoses.resize(frameCount);

  for (auto &keyPose : clip.transformKeyPoses) {
    expect("key_pose");
    keyPose.blenderFrame = readInt();

    expect("location");
    keyPose.location = readVec3();

    const std::string rotationToken = next();
    if (rotationToken == "rotation_quaternion") {
      keyPose.rotation = readQuat();
    } else if (rotationToken == "rotation") {
      keyPose.rotation = quatFromEulerXYZ(readVec3());
    } else {
      throw std::runtime_error("Expected rotation token, got '" +
                               rotationToken + "'");
    }

    expect("scale");
    keyPose.scale = readVec3();
  }
}

static BlenderModel loadAnyModel(const std::string &path) {
  std::ifstream file(path);

  if (!file) {
    throw std::runtime_error("Could not open file: " + path);
  }

  currentTokens.clear();
  cursor = 0;

  BlenderModel model;

  currentTokens = readTokensIgnoringComments(file);

  const std::string magic = readString();
  const int version = readInt();
  const bool isLegacyVertex = magic == "spaceguy_3d" && version == 2;
  const bool isUnified =
      magic == "spaceguy_3d" && (version >= 3 && version <= 6);
  const bool hasAnimatedNormals = magic == "spaceguy_3d" && version >= 6;
  const bool hasAttachments = magic == "spaceguy_3d" && version >= 4;
  const bool hasNormals = magic == "spaceguy_3d" && version >= 5;
  const bool isLegacyTransform =
      magic == "spaceguy_3d_transform" && (version == 1 || version == 2);

  if (!isLegacyVertex && !isUnified && !isLegacyTransform) {
    throw std::runtime_error("Unsupported .3d version");
  }

  expect("object_name");
  model.name = readString();

  expect("fps");
  model.fps = readFloat();

  expect("vertex_count");
  const std::size_t vertexCount = readSize();

  expect("index_count");
  const std::size_t indexCount = readSize();

  expect("animation_count");
  const std::size_t animationCount = readSize();

  expect("vertices");

  model.vertices.resize(vertexCount);

  for (auto &vertex : model.vertices) {
    vertex.pos.x = readFloat();
    vertex.pos.y = readFloat();
    vertex.pos.z = readFloat();

    vertex.color.x = readFloat();
    vertex.color.y = readFloat();
    vertex.color.z = readFloat();

    if (hasNormals) {
      vertex.normal.x = readFloat();
      vertex.normal.y = readFloat();
      vertex.normal.z = readFloat();
    }
  }

  expect("indices");

  model.indices.resize(indexCount);

  for (auto &index : model.indices) {
    index = readUInt32();
  }

  if (!hasNormals) {
    for (std::size_t i = 0; i + 2 < model.indices.size(); i += 3) {
      Vertex &a = model.vertices[model.indices[i]];
      Vertex &b = model.vertices[model.indices[i + 1]];
      Vertex &c = model.vertices[model.indices[i + 2]];
      const glm::vec3 faceNormal = glm::cross(b.pos - a.pos, c.pos - a.pos);
      a.normal += faceNormal;
      b.normal += faceNormal;
      c.normal += faceNormal;
    }

    for (Vertex &vertex : model.vertices) {
      const float lengthSquared = glm::dot(vertex.normal, vertex.normal);
      vertex.normal = lengthSquared > 0.0f
                          ? vertex.normal / std::sqrt(lengthSquared)
                          : glm::vec3{0.0f, 0.0f, 1.0f};
    }
  }

  expect("animations");

  model.animations.reserve(animationCount);

  for (std::size_t animationIndex = 0; animationIndex < animationCount;
       ++animationIndex) {
    AnimationClip clip;

    expect("animation");
    clip.name = readString();

    if (isUnified) {
      expect("type");
      clip.kind = readAnimationKind();
    } else if (isLegacyTransform) {
      clip.kind = AnimationKind::Transform;
    } else {
      clip.kind = AnimationKind::Vertex;
    }

    readClipHeader(clip);
    const std::size_t frameCount = readSize();

    if (clip.kind == AnimationKind::Vertex) {
      readVertexKeyPoses(clip, vertexCount, frameCount, hasAnimatedNormals);
      if (hasAttachments) {
        readAttachments(clip);
      }
    } else {
      readTransformKeyPoses(clip, frameCount);
    }

    model.animations.push_back(std::move(clip));
  }

  currentTokens = {};
  cursor = 0;

  return model;
}

BlenderModel loadModel(const std::string &path) { return loadAnyModel(path); }

static ImporterTransform readTransform() {
  ImporterTransform transform{};

  expect("position");
  transform.position = readVec3();

  expect("rotation");
  transform.rotation = readVec3();

  expect("scale");
  transform.scale = readVec3();

  return transform;
}

WorldData loadWorldData() {
  std::ifstream file("assets/world.world");

  if (!file) {
    throw std::runtime_error("Could not open file: assets/world.world");
  }

  currentTokens.clear();
  cursor = 0;
  currentTokens = readTokensIgnoringComments(file);

  WorldData data{};

  expect("spaceguy_world");

  const int version = readInt();

  if (version != 1) {
    throw std::runtime_error("Unsupported .world version");
  }

  expect("floor");
  data.floor = readTransform();

  expect("floor_details");
  data.floor_details = readTransform();

  expect("water");
  data.water = readTransform();

  expect("water_details");
  data.water_details = readTransform();

  expect("camera");
  data.camera.transform = readTransform();

  expect("look_direction");
  data.camera.direction = readVec3();

  expect("fov_y");
  data.camera.fovY = readFloat();

  expect("clip_start");
  data.camera.clipStart = readFloat();

  expect("clip_end");
  data.camera.clipEnd = readFloat();

  expect("wizards");

  expect("wizard_count");
  data.wizards.count = readInt();

  data.wizards.positions.resize(static_cast<std::size_t>(data.wizards.count));

  for (glm::vec3 &position : data.wizards.positions) {
    position = readVec3();
  }

  expect("ogres");

  expect("ogre_count");
  data.ogres.count = readInt();

  data.ogres.positions.resize(static_cast<std::size_t>(data.ogres.count));

  for (glm::vec3 &position : data.ogres.positions) {
    position = readVec3();
  }

  currentTokens = {};
  cursor = 0;

  return data;
}

BlenderTransformModel loadTransformModel(const std::string &path) {
  BlenderModel source = loadAnyModel(path);

  BlenderTransformModel model{};
  model.name = source.name;
  model.fps = source.fps;
  model.vertices = std::move(source.vertices);
  model.indices = std::move(source.indices);

  for (const AnimationClip &sourceClip : source.animations) {
    if (sourceClip.kind != AnimationKind::Transform) {
      continue;
    }

    TransformAnimationClip clip{};
    clip.name = sourceClip.name;
    clip.startFrame = sourceClip.startFrame;
    clip.endFrame = sourceClip.endFrame;
    clip.keyPoses = sourceClip.transformKeyPoses;
    clip.loop = sourceClip.loop;
    model.animations.push_back(std::move(clip));
  }

  return model;
}
