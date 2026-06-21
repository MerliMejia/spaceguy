#pragma once

#include "../utils/types.h"

enum class ObjectRenderKind { Static, Animated, TransformAnimated };

enum class ObjectWorldKind { None, Floor, Wizard };

struct Renderable {
  int entity = -1;
  ObjectRenderKind renderKind = ObjectRenderKind::Static;
  const Mesh *mesh = nullptr;
  const AnimatedMesh *animatedMesh = nullptr;
  const TransformAnimatedMesh *transformAnimatedMesh = nullptr;
  bool visible = true;
};

struct TransformComponent {
  int entity = -1;
  glm::mat4 model{1.0f};
  glm::mat4 baseModel{1.0f};
};

struct AnimationComponent {
  int entity = -1;
  uint32_t activeAnimation = 0;
  uint32_t activeFrame = 0;
  float animationTimeSeconds = 0.0f;
  float animationPlaySpeed = 1.0f;
};

struct WorldComponent {
  int entity = -1;
  ObjectWorldKind worldKind = ObjectWorldKind::None;
  int worldEntityId = -1;
};

struct ProjectileComponent {
  int entity = -1;
  glm::vec3 direction{0.0f, -1.0f, 0.0f};
  float speed = 20.0f;
  float timeAlive = 0.0f;
};

struct Resources {
  std::vector<Renderable> renderables;
  std::vector<TransformComponent> transforms;
  std::vector<AnimationComponent> animations;
  std::vector<WorldComponent> worlds;
  std::vector<ProjectileComponent> projectiles;
};

extern Resources resources;

int createEntity();
bool isEntityAlive(int entity);

Renderable &addRenderable(int entity);
Renderable &getRenderable(int entity);
Renderable *tryGetRenderable(int entity);

TransformComponent &addTransform(int entity);
TransformComponent &getTransform(int entity);
TransformComponent *tryGetTransform(int entity);

AnimationComponent &addAnimation(int entity);
AnimationComponent &getAnimation(int entity);
AnimationComponent *tryGetAnimation(int entity);

WorldComponent &addWorld(int entity);
WorldComponent &getWorld(int entity);
WorldComponent *tryGetWorld(int entity);

ProjectileComponent &addProjectile(int entity);
ProjectileComponent &getProjectile(int entity);
ProjectileComponent *tryGetProjectile(int entity);

void destroyEntity(int entity);
void processDestroyQueue();
