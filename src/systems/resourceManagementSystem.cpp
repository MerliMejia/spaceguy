#include "resourceManagementSystem.h"
#include <concepts>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static std::unordered_map<int, int> entityToRenderables;
static std::unordered_map<int, int> entityToTransforms;
static std::unordered_map<int, int> entityToAnimations;
static std::unordered_map<int, int> entityToProjectiles;
static std::unordered_map<int, int> entityToWizardBehaviors;
static std::unordered_map<int, int> entityToParticleEmitterCpuComponents;
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

template <typename T>
concept EntityComponent = requires(T component, int entity) {
  T{.entity = entity};
  { component.entity } -> std::convertible_to<int>;
};

template <EntityComponent T>
T &addComponent(int entity, std::unordered_map<int, int> &map,
                std::vector<T> &list, std::string_view componentName) {
  if (!alive.contains(entity)) {
    throw std::runtime_error("The entity " + std::to_string(entity) +
                             " doesn't live anymore");
  }

  auto it = map.find(entity);
  if (it != map.end()) {
    throw std::runtime_error("The entity " + std::to_string(entity) +
                             " already has a " + std::string(componentName));
  }

  int tIndex = static_cast<int>(list.size());
  list.push_back(T{.entity = entity});
  map[entity] = tIndex;

  return list[tIndex];
}

template <EntityComponent T>
T &getComponent(int entity, std::unordered_map<int, int> &map,
                std::vector<T> &list, std::string_view componentName) {
  auto it = map.find(entity);

  if (it != map.end()) {
    return list[it->second];
  }

  throw std::runtime_error("Entity doesn't have a " +
                           std::string(componentName));
}

template <EntityComponent T>
T *tryGetComponent(int entity, std::unordered_map<int, int> &map,
                   std::vector<T> &list) {
  auto it = map.find(entity);

  if (it != map.end()) {
    return &list[it->second];
  }

  return nullptr;
}

template <EntityComponent T>
void destroyComponent(int entity, std::unordered_map<int, int> &map,
                      std::vector<T> &list) {
  auto it = map.find(entity);
  if (it == map.end()) {
    return;
  }

  int indexToRemove = it->second;
  int lastIndex = static_cast<int>(list.size()) - 1;

  if (indexToRemove != lastIndex) {
    T moved = list[lastIndex];

    list[indexToRemove] = moved;
    map[moved.entity] = indexToRemove;
  }

  list.pop_back();
  map.erase(entity);
}

Renderable &addRenderable(int entity) {
  return addComponent<Renderable>(entity, entityToRenderables,
                                  resources.renderables, "renderable");
}

template <> Renderable &addComponent<Renderable>(int entity) {
  return addRenderable(entity);
}

Renderable &getRenderable(int entity) {
  return getComponent<Renderable>(entity, entityToRenderables,
                                  resources.renderables, "renderable");
}

Renderable *tryGetRenderable(int entity) {
  return tryGetComponent<Renderable>(entity, entityToRenderables,
                                     resources.renderables);
}

TransformComponent &addTransform(int entity) {
  return addComponent<TransformComponent>(entity, entityToTransforms,
                                          resources.transforms, "transform");
}

template <> TransformComponent &addComponent<TransformComponent>(int entity) {
  return addTransform(entity);
}

TransformComponent &getTransform(int entity) {
  return getComponent<TransformComponent>(entity, entityToTransforms,
                                          resources.transforms, "transform");
}

TransformComponent *tryGetTransform(int entity) {
  return tryGetComponent<TransformComponent>(entity, entityToTransforms,
                                             resources.transforms);
}

AnimationComponent &addAnimation(int entity) {
  return addComponent<AnimationComponent>(entity, entityToAnimations,
                                          resources.animations, "animation");
}

template <> AnimationComponent &addComponent<AnimationComponent>(int entity) {
  return addAnimation(entity);
}

AnimationComponent &getAnimation(int entity) {
  return getComponent<AnimationComponent>(entity, entityToAnimations,
                                          resources.animations, "animation");
}

AnimationComponent *tryGetAnimation(int entity) {
  return tryGetComponent<AnimationComponent>(entity, entityToAnimations,
                                             resources.animations);
}

ProjectileComponent &addProjectile(int entity) {
  return addComponent<ProjectileComponent>(entity, entityToProjectiles,
                                           resources.projectiles,
                                           "projectile component");
}

template <> ProjectileComponent &addComponent<ProjectileComponent>(int entity) {
  return addProjectile(entity);
}

ProjectileComponent &getProjectile(int entity) {
  return getComponent<ProjectileComponent>(entity, entityToProjectiles,
                                           resources.projectiles,
                                           "projectile component");
}

ProjectileComponent *tryGetProjectile(int entity) {
  return tryGetComponent<ProjectileComponent>(entity, entityToProjectiles,
                                              resources.projectiles);
}

WizardBehaviorComponent &addWizardBehavior(int entity) {
  return addComponent<WizardBehaviorComponent>(entity, entityToWizardBehaviors,
                                               resources.wizardBehaviors,
                                               "wizard behavior component");
}

template <>
WizardBehaviorComponent &addComponent<WizardBehaviorComponent>(int entity) {
  return addWizardBehavior(entity);
}

WizardBehaviorComponent &getWizardBehavior(int entity) {
  return getComponent<WizardBehaviorComponent>(entity, entityToWizardBehaviors,
                                               resources.wizardBehaviors,
                                               "wizard behavior component");
}

WizardBehaviorComponent *tryGetWizardBehavior(int entity) {
  return tryGetComponent<WizardBehaviorComponent>(
      entity, entityToWizardBehaviors, resources.wizardBehaviors);
}

ParticleEmitterCpuComponent &addParticleEmitterCpuComponent(int entity) {
  return addComponent<ParticleEmitterCpuComponent>(
      entity, entityToParticleEmitterCpuComponents,
      resources.particleEmitterCpuComponents, "particle emitter cpu component");
}

template <>
ParticleEmitterCpuComponent &addComponent<ParticleEmitterCpuComponent>(
    int entity) {
  return addParticleEmitterCpuComponent(entity);
}

ParticleEmitterCpuComponent &getParticleEmitterCpuComponent(int entity) {
  return getComponent<ParticleEmitterCpuComponent>(
      entity, entityToParticleEmitterCpuComponents,
      resources.particleEmitterCpuComponents, "particle emitter cpu component");
}

ParticleEmitterCpuComponent *tryGetParticleEmitterCpuComponent(int entity) {
  return tryGetComponent<ParticleEmitterCpuComponent>(
      entity, entityToParticleEmitterCpuComponents,
      resources.particleEmitterCpuComponents);
}

void destroyEntity(int entity) { destroyQueue.push_back(entity); }

static void destroyRenderable(int entity) {
  destroyComponent<Renderable>(entity, entityToRenderables,
                               resources.renderables);
}

static void destroyTransform(int entity) {
  destroyComponent<TransformComponent>(entity, entityToTransforms,
                                       resources.transforms);
}

static void destroyAnimation(int entity) {
  destroyComponent<AnimationComponent>(entity, entityToAnimations,
                                       resources.animations);
}

static void destroyProjectile(int entity) {
  destroyComponent<ProjectileComponent>(entity, entityToProjectiles,
                                        resources.projectiles);
}

static void destroyWizardBehavior(int entity) {
  destroyComponent<WizardBehaviorComponent>(entity, entityToWizardBehaviors,
                                            resources.wizardBehaviors);
}

static void destroyParticleEmitterCpuComponent(int entity) {
  destroyComponent<ParticleEmitterCpuComponent>(
      entity, entityToParticleEmitterCpuComponents,
      resources.particleEmitterCpuComponents);
}

void processDestroyQueue() {
  for (int entity : destroyQueue) {
    if (!alive.contains(entity)) {
      continue;
    }
    destroyRenderable(entity);
    destroyTransform(entity);
    destroyAnimation(entity);
    destroyProjectile(entity);
    destroyWizardBehavior(entity);
    destroyParticleEmitterCpuComponent(entity);
    alive.erase(entity);
  }

  destroyQueue.clear();
}
