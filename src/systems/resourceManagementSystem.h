#pragma once

#include "../utils/types.h"
#include <vector>

enum class ObjectRenderKind { Static, Animated, TransformAnimated };

enum class BehaviorKind { None, Wizard };

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

struct ProjectileComponent {
  int entity = -1;
  int ownerEntity = -1;
  int travelEffectEmitter = -1;
  glm::vec3 direction{0.0f, -1.0f, 0.0f};
  float speed = 20.0f;
  float timeAlive = 0.0f;
};

enum class WizzardState {
  None,
  ChoosingNewPosition,
  MovingToPosition,
  Attacking,
  Waiting,
  Evading,
  Recovering
};

struct WizardBehaviorComponent {
  int entity = -1;
  WizzardState state = WizzardState::None;
  glm::vec2 nextPos{0.0f};
  glm::vec4 debugColor{1.0f};
  float speed = 8.0f;

  float stamina = 1.0f;
  float maxStamina = 1.0f;
  float staminaRecoverPerSecond = 0.35f;
  float moveStaminaPerSecond = 0.18f;
  float evadeStaminaCost = 0.28f;
  float attackStaminaCost = 0.32f;

  glm::vec2 evadeDir{0.0f};
  glm::vec2 previousPosition{0.0f};
  glm::vec2 velocity{0.0f};

  int nextAttackEntity = -1;
  float attackTime = 0;

  int attackCounter = 0;
  int shootEffecEntity = -1;
};

struct ParticleEmitterCpuComponent {
  int entity = -1;
  int owner = -1;
  uint32_t firstParticle = 0;
  uint32_t maxParticles = 0;
  bool active = true;
  float spawnRate = 0.0f;
  float spawnAccumulator = 0.0f;
  float particleLifetime = 1.0f;
  float particleStartSize = 0.1f;
  float particleEndSize = 0.1f;
  float spawnSpeed = 1.0f;
  float maxColorSpeed = 1.0f;
  glm::vec4 lifeColorStart{1.0f};
  glm::vec4 lifeColorEnd{1.0f};
  glm::vec4 speedColorSlow{1.0f};
  glm::vec4 speedColorFast{1.0f};
  glm::vec3 position{0.0f};
  glm::vec3 direction{0.0f, 0.0f, 1.0f};
  ParticleEmitterShape shape = ParticleEmitterShape::Cone;
};

struct Resources {
  std::vector<Renderable> renderables;
  std::vector<TransformComponent> transforms;
  std::vector<AnimationComponent> animations;
  std::vector<ProjectileComponent> projectiles;
  std::vector<WizardBehaviorComponent> wizardBehaviors;
  std::vector<ParticleEmitterCpuComponent> particleEmitterCpuComponents;
};

extern Resources resources;

int createEntity();
bool isEntityAlive(int entity);

template <typename T> T &addComponent(int entity);

Renderable &addRenderable(int entity);
Renderable &getRenderable(int entity);
Renderable *tryGetRenderable(int entity);

TransformComponent &addTransform(int entity);
TransformComponent &getTransform(int entity);
TransformComponent *tryGetTransform(int entity);

AnimationComponent &addAnimation(int entity);
AnimationComponent &getAnimation(int entity);
AnimationComponent *tryGetAnimation(int entity);

ProjectileComponent &addProjectile(int entity);
ProjectileComponent &getProjectile(int entity);
ProjectileComponent *tryGetProjectile(int entity);

WizardBehaviorComponent &addWizardBehavior(int entity);
WizardBehaviorComponent &getWizardBehavior(int entity);
WizardBehaviorComponent *tryGetWizardBehavior(int entity);

ParticleEmitterCpuComponent &addParticleEmitterCpuComponent(int entity);
ParticleEmitterCpuComponent &getParticleEmitterCpuComponent(int entity);
ParticleEmitterCpuComponent *tryGetParticleEmitterCpuComponent(int entity);

void destroyEntity(int entity);
void processDestroyQueue();
