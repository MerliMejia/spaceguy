#include "resourceManagementSystem.h"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static std::unordered_map<int, int> entityToRenderables;
static std::unordered_map<int, int> entityToTransforms;
static std::unordered_map<int, int> entityToAnimations;
static std::unordered_map<int, int> entityToBehaviors;
static std::unordered_map<int, int> entityToProjectiles;
static int nextEntityId = 1;

static std::unordered_set<int> alive;
Resources resources{};

static std::vector<int> destroyQueue;

int createEntity() {
  int next = nextEntityId++;
  alive.insert(next);

  return next;
}

bool isEntityAlive(int entity) { return alive.contains(entity); }

Renderable &addRenderable(int entity) {
  if (!alive.contains(entity)) {
    throw std::runtime_error("This entity doesn't live anymore");
  }

  auto it = entityToRenderables.find(entity);
  if (it != entityToRenderables.end()) {
    throw std::runtime_error("This entity already has a renderable " +
                             std::to_string(entity));
  }

  int renderableIndex = resources.renderables.size();
  resources.renderables.push_back(Renderable{.entity = entity});
  entityToRenderables[entity] = renderableIndex;

  return resources.renderables[renderableIndex];
}

Renderable &getRenderable(int entity) {
  auto r = entityToRenderables.find(entity);

  if (r != entityToRenderables.end()) {
    return resources.renderables[r->second];
  }

  throw std::runtime_error("Entity doesn't have a renderable");
}

Renderable *tryGetRenderable(int entity) {
  auto r = entityToRenderables.find(entity);

  if (r != entityToRenderables.end()) {
    return &resources.renderables[r->second];
  }

  return nullptr;
}

TransformComponent &addTransform(int entity) {
  if (!alive.contains(entity)) {
    throw std::runtime_error("This entity doesn't live anymore");
  }

  auto it = entityToTransforms.find(entity);
  if (it != entityToTransforms.end()) {
    throw std::runtime_error("This entity already has a transform " +
                             std::to_string(entity));
  }

  int transformIndex = resources.transforms.size();
  resources.transforms.push_back(TransformComponent{.entity = entity});
  entityToTransforms[entity] = transformIndex;

  return resources.transforms[transformIndex];
}

TransformComponent &getTransform(int entity) {
  auto r = entityToTransforms.find(entity);

  if (r != entityToTransforms.end()) {
    return resources.transforms[r->second];
  }

  throw std::runtime_error("Entity doesn't have a transform");
}

TransformComponent *tryGetTransform(int entity) {
  auto r = entityToTransforms.find(entity);

  if (r != entityToTransforms.end()) {
    return &resources.transforms[r->second];
  }

  return nullptr;
}

AnimationComponent &addAnimation(int entity) {
  if (!alive.contains(entity)) {
    throw std::runtime_error("This entity doesn't live anymore");
  }

  auto it = entityToAnimations.find(entity);
  if (it != entityToAnimations.end()) {
    throw std::runtime_error("This entity already has an animation " +
                             std::to_string(entity));
  }

  int animationIndex = resources.animations.size();
  resources.animations.push_back(AnimationComponent{.entity = entity});
  entityToAnimations[entity] = animationIndex;

  return resources.animations[animationIndex];
}

AnimationComponent &getAnimation(int entity) {
  auto r = entityToAnimations.find(entity);

  if (r != entityToAnimations.end()) {
    return resources.animations[r->second];
  }

  throw std::runtime_error("Entity doesn't have an animation");
}

AnimationComponent *tryGetAnimation(int entity) {
  auto r = entityToAnimations.find(entity);

  if (r != entityToAnimations.end()) {
    return &resources.animations[r->second];
  }

  return nullptr;
}

BehaviorComponent &addBehavior(int entity) {
  if (!alive.contains(entity)) {
    throw std::runtime_error("This entity doesn't live anymore");
  }

  auto it = entityToBehaviors.find(entity);
  if (it != entityToBehaviors.end()) {
    throw std::runtime_error("This entity already has a behavior component " +
                             std::to_string(entity));
  }

  int behaviorIndex = resources.behaviors.size();
  resources.behaviors.push_back(BehaviorComponent{.entity = entity});
  entityToBehaviors[entity] = behaviorIndex;

  return resources.behaviors[behaviorIndex];
}

BehaviorComponent &getBehavior(int entity) {
  auto r = entityToBehaviors.find(entity);

  if (r != entityToBehaviors.end()) {
    return resources.behaviors[r->second];
  }

  throw std::runtime_error("Entity doesn't have a behavior component");
}

BehaviorComponent *tryGetBehavior(int entity) {
  auto r = entityToBehaviors.find(entity);

  if (r != entityToBehaviors.end()) {
    return &resources.behaviors[r->second];
  }

  return nullptr;
}

ProjectileComponent &addProjectile(int entity) {
  if (!alive.contains(entity)) {
    throw std::runtime_error("This entity doesn't live anymore");
  }

  auto it = entityToProjectiles.find(entity);
  if (it != entityToProjectiles.end()) {
    throw std::runtime_error("This entity already has a projectile component " +
                             std::to_string(entity));
  }

  int projectileIndex = resources.projectiles.size();
  resources.projectiles.push_back(ProjectileComponent{.entity = entity});
  entityToProjectiles[entity] = projectileIndex;

  return resources.projectiles[projectileIndex];
}

ProjectileComponent &getProjectile(int entity) {
  auto r = entityToProjectiles.find(entity);

  if (r != entityToProjectiles.end()) {
    return resources.projectiles[r->second];
  }

  throw std::runtime_error("Entity doesn't have a projectile component");
}

ProjectileComponent *tryGetProjectile(int entity) {
  auto r = entityToProjectiles.find(entity);

  if (r != entityToProjectiles.end()) {
    return &resources.projectiles[r->second];
  }

  return nullptr;
}

void destroyEntity(int entity) { destroyQueue.push_back(entity); }

static void destroyRenderable(int entity) {
  auto it = entityToRenderables.find(entity);
  if (it == entityToRenderables.end()) {
    return;
  }
  int rIndexToRemove = it->second;
  int rLastIndex = resources.renderables.size() - 1;

  if (rIndexToRemove != rLastIndex) {
    Renderable moved = resources.renderables[rLastIndex];

    resources.renderables[rIndexToRemove] = moved;
    entityToRenderables[moved.entity] = rIndexToRemove;
  }

  resources.renderables.pop_back();
  entityToRenderables.erase(entity);
}

static void destroyTransform(int entity) {
  auto it = entityToTransforms.find(entity);
  if (it == entityToTransforms.end()) {
    return;
  }
  int indexToRemove = it->second;
  int lastIndex = resources.transforms.size() - 1;

  if (indexToRemove != lastIndex) {
    TransformComponent moved = resources.transforms[lastIndex];

    resources.transforms[indexToRemove] = moved;
    entityToTransforms[moved.entity] = indexToRemove;
  }

  resources.transforms.pop_back();
  entityToTransforms.erase(entity);
}

static void destroyAnimation(int entity) {
  auto it = entityToAnimations.find(entity);
  if (it == entityToAnimations.end()) {
    return;
  }
  int indexToRemove = it->second;
  int lastIndex = resources.animations.size() - 1;

  if (indexToRemove != lastIndex) {
    AnimationComponent moved = resources.animations[lastIndex];

    resources.animations[indexToRemove] = moved;
    entityToAnimations[moved.entity] = indexToRemove;
  }

  resources.animations.pop_back();
  entityToAnimations.erase(entity);
}

static void destroyBehavior(int entity) {
  auto it = entityToBehaviors.find(entity);
  if (it == entityToBehaviors.end()) {
    return;
  }
  int indexToRemove = it->second;
  int lastIndex = resources.behaviors.size() - 1;

  if (indexToRemove != lastIndex) {
    BehaviorComponent moved = resources.behaviors[lastIndex];

    resources.behaviors[indexToRemove] = moved;
    entityToBehaviors[moved.entity] = indexToRemove;
  }

  resources.behaviors.pop_back();
  entityToBehaviors.erase(entity);
}

static void destroyProjectile(int entity) {
  auto it = entityToProjectiles.find(entity);
  if (it == entityToProjectiles.end()) {
    return;
  }
  int indexToRemove = it->second;
  int lastIndex = resources.projectiles.size() - 1;

  if (indexToRemove != lastIndex) {
    ProjectileComponent moved = resources.projectiles[lastIndex];

    resources.projectiles[indexToRemove] = moved;
    entityToProjectiles[moved.entity] = indexToRemove;
  }

  resources.projectiles.pop_back();
  entityToProjectiles.erase(entity);
}

void processDestroyQueue() {
  for (int entity : destroyQueue) {
    if (!alive.contains(entity)) {
      continue;
    }
    destroyRenderable(entity);
    destroyTransform(entity);
    destroyAnimation(entity);
    destroyBehavior(entity);
    destroyProjectile(entity);
    alive.erase(entity);
  }

  destroyQueue.clear();
}
