#include "projectileSystem.h"
#include "../engine/blender/importer.h"
#include "../engine/vulkanRenderer.h"
#include "../utils/generators.h"
#include "../utils/math.h"
#include "../utils/time.h"
#include "../utils/types.h"
#include "glm/fwd.hpp"
#include "particleLifeSystem.h"
#include "resourceManagementSystem.h"
#include "sceneContext.h"
#include "spacialGridHashSystem.h"
#include <iostream>
#include <unordered_set>

static std::unordered_map<int, float> previousProjectileDepths;

static Mesh wizardProjectileMesh;
std::unordered_set<int> projectileHits;

const constexpr float timeToDie = 5.0f;
const constexpr float explosionLightIntensityMultiplier = 2.0f;

static void destroyProjectileWithEffects(int projectileEntity) {
  ProjectileComponent *projectile = tryGetProjectile(projectileEntity);
  if (projectile == nullptr) {
    return;
  }

  const int emitterEntity = projectile->travelEffectEmitter;
  if (emitterEntity == -1) {
    return;
  }

  // Mark teardown immediately so another collision in this frame cannot
  // schedule the same emitter and light a second time.
  projectile->travelEffectEmitter = -1;

  if (ParticleEmitterCpuComponent *emitter =
          tryGetParticleEmitterCpuComponent(emitterEntity)) {
    const int lightEntity = emitter->lightEntity;
    emitter->lightEntity = -1;

    if (lightEntity != -1) {
      destroyEntity(lightEntity);
    }

    addParticleEmitterToBeDestroyed(emitterEntity);
  }

  destroyEntity(projectileEntity);
}

static int takeProjectileLight(int projectileEntity) {
  ProjectileComponent *projectile = tryGetProjectile(projectileEntity);
  if (projectile == nullptr || projectile->travelEffectEmitter == -1) {
    return -1;
  }

  ParticleEmitterCpuComponent *emitter =
      tryGetParticleEmitterCpuComponent(projectile->travelEffectEmitter);
  if (emitter == nullptr) {
    return -1;
  }

  const int lightEntity = emitter->lightEntity;
  emitter->lightEntity = -1;
  return lightEntity;
}

void initializeProjectiles() {
  BlenderModel wizardProjectile = loadModel("assets/Wizard_Projectile.3d");
  wizardProjectileMesh =
      generateMesh(wizardProjectile.vertices, wizardProjectile.indices);
}

void spawnWizardProjectile(int wizard, int lightEntity) {
  TransformComponent tc = getTransform(wizard);
  Transform wizardTransform = modelToTransform(tc.model);

  glm::vec3 localForward{0.0f, -1.0f, 0.0f};
  glm::vec3 forward = glm::normalize(wizardTransform.rotation * localForward);

  spawnWizardProjectile(wizard, forward, lightEntity);
}

void spawnWizardProjectile(int wizard, glm::vec3 direction, int lightEntity) {
  TransformComponent tc = getTransform(wizard);
  Transform wizardTransform = modelToTransform(tc.model);

  glm::vec3 forward = glm::normalize(direction);

  float spawnDistance = 1.0f;
  glm::vec3 initialPos = wizardTransform.position + forward * spawnDistance;

  int projectile = createEntity();

  TransformComponent &ptc = addTransform(projectile);
  Transform pt = modelToTransform(ptc.model);
  pt.position = initialPos;
  pt.scale = glm::vec3{0.3f};
  ptc.model = transformToModel(pt.position, pt.rotation, pt.scale);

  ProjectileComponent &projectileComponent = addProjectile(projectile);
  projectileComponent.direction = forward;
  projectileComponent.ownerEntity = wizard;

  int travelEffectEmitterEntity = createEntity();

  ParticleEmitterCpuComponent &emitter =
      addParticleEmitterCpuComponent(travelEffectEmitterEntity);

  generateLongTrailParticleEmitter(emitter, initialPos, forward);

  int particleLightEntity = lightEntity;
  PointLightComponent *lightComponent = nullptr;

  if (particleLightEntity != -1 && isEntityAlive(particleLightEntity)) {
    lightComponent = tryGetPointLight(particleLightEntity);
  }

  if (lightComponent == nullptr) {
    particleLightEntity = createEntity();
    lightComponent = &addPointLight(particleLightEntity);
    lightComponent->color = {1.0f, 0.0f, 0.0f};
    lightComponent->intensity = 0.1f;
    lightComponent->attenuation = glm::vec3{0.01f, 0.01f, 0.01f};
  }

  lightComponent->position = initialPos;

  // At some point should be a constant
  emitter.lifeColorStart = {1.0f, 0.0f, 0.0f, 1.0f};
  emitter.lifeColorEnd = {0.0f, 0.0f, 1.0f, 1.0f};

  emitter.speedColorSlow = {1.0f, 0.0f, 0.0f, 1.0f};
  emitter.speedColorFast = {1.0f, 0.0f, 0.0f, 1.0f};

  emitter.particleStartSize = 0.12;
  emitter.particleEndSize = 0.01;
  emitter.entity = travelEffectEmitterEntity;

  emitter.owner = projectile;
  emitter.lightEntity = particleLightEntity;
  projectileComponent.travelEffectEmitter = travelEffectEmitterEntity;
}

void updateProjectiles() {
  for (ProjectileComponent &projectile : resources.projectiles) {
    if (projectile.travelEffectEmitter == -1) {
      continue;
    }

    if (projectile.timeAlive >= timeToDie) {
      destroyProjectileWithEffects(projectile.entity);
      continue;
    }

    projectile.timeAlive += timeState.deltaTime;

    TransformComponent &tc = getTransform(projectile.entity);
    Transform transform = modelToTransform(tc.model);

    // Exclude if goes outside the camera
    const glm::vec3 toCamera = transform.position - sceneContext.cameraPosition;

    constexpr float cameraExclusionRadius = 1.0f;

    if (glm::dot(toCamera, toCamera) <
        cameraExclusionRadius * cameraExclusionRadius) {
      destroyProjectileWithEffects(projectile.entity);
      continue;
    }

    transform.position +=
        projectile.direction * projectile.speed * timeState.deltaTime;

    tc.model = transformToModel(transform.position, transform.rotation,
                                transform.scale);

    ParticleEmitterCpuComponent &travelEmitter =
        getParticleEmitterCpuComponent(projectile.travelEffectEmitter);

    travelEmitter.position = transform.position;
    travelEmitter.direction = projectile.direction;

    PointLightComponent &particleLight =
        getPointLight(travelEmitter.lightEntity);
    particleLight.position = transform.position;

    // Check hits
    CellCoord center =
        worldToCell(transform.position, spacialGridContext.cellWidth,
                    spacialGridContext.cellHeight);

    executeOnNearbyCells(center, [transform, projectile](int closeEntity) {
      auto explodeAtProjectile = [&](int lightEntity,
                                     int secondaryLightEntity = -1) {
        int explosionEffect = createEntity();

        ParticleEmitterCpuComponent &emitter =
            addParticleEmitterCpuComponent(explosionEffect);

        generateExplosionParticleEmitter(emitter, transform.position, 2);

        emitter.lifeColorStart = {1.0f, 0.0f, 0.0f, 1.0f};
        emitter.lifeColorEnd = {0.0f, 0.0f, 1.0f, 1.0f};

        emitter.speedColorSlow = {1.0f, 0.0f, 0.0f, 1.0f};
        emitter.speedColorFast = {1.0f, 0.0f, 0.0f, 1.0f};
        emitter.entity = explosionEffect;
        emitter.lightEntity = lightEntity;
        emitter.secondaryLightEntity = secondaryLightEntity;

        if (PointLightComponent *light = tryGetPointLight(lightEntity)) {
          light->position = transform.position;
          light->intensity *= explosionLightIntensityMultiplier;
          emitter.lightStartIntensity = light->intensity;
          emitter.lightTargetIntensity = 0.0f;
        }
        if (PointLightComponent *light =
                tryGetPointLight(secondaryLightEntity)) {
          light->position = transform.position;
          light->intensity *= explosionLightIntensityMultiplier;
          emitter.secondaryLightStartIntensity = light->intensity;
          emitter.secondaryLightTargetIntensity = 0.0f;
        }

        addParticleEmitterToBeDestroyed(explosionEffect, 0.2f);
      };

      // Wizards
      if (WizardBehaviorComponent *wizard = tryGetWizardBehavior(closeEntity)) {
        if (wizard->entity != projectile.ownerEntity) {
          TransformComponent &wtc = getTransform(closeEntity);
          const Transform &wt = modelToTransform(wtc.model);

          float distanceSqr = getDistanceSqr(glm::vec2{transform.position},
                                             glm::vec2{wt.position});

          if (distanceSqr <= 1.0f) {
            const int explosionLight = takeProjectileLight(projectile.entity);
            explodeAtProjectile(explosionLight);

            destroyEntity(wizard->shootEffecEntity);
            destroyEntity(closeEntity);
            destroyProjectileWithEffects(projectile.entity);

            return ExecuteOnNearbyCellsStatus::Done;
          }
        }
      }

      // Ogres
      if (OgreBehaviorComponent *ogre =
              tryGetOgreBehaviorComponent(closeEntity)) {
        if (ogre->entity != projectile.ownerEntity) {
          TransformComponent &ogreTransform = getTransform(ogre->entity);
          const Transform &ogreWorld = modelToTransform(ogreTransform.model);

          const float distanceSqr = getDistanceSqr(
              glm::vec2{transform.position}, glm::vec2{ogreWorld.position});

          if (distanceSqr <= 1.0f) {
            const int explosionLight = takeProjectileLight(projectile.entity);

            explodeAtProjectile(explosionLight);

            // Need to add logic for taking damage instead of immediatly dying
            // destroyEntity(ogre->entity);
            // destroyEntity(ogre->bladeEntity);
            destroyProjectileWithEffects(projectile.entity);

            return ExecuteOnNearbyCellsStatus::Done;
          }
        }
      }

      // Projectiles
      if (ProjectileComponent *otherProjectile =
              tryGetProjectile(closeEntity)) {
        if (otherProjectile->entity != projectile.entity &&
            otherProjectile->travelEffectEmitter != -1 &&
            otherProjectile->ownerEntity != projectile.ownerEntity) {
          TransformComponent &otc = getTransform(closeEntity);
          const Transform &ot = modelToTransform(otc.model);

          float distanceSqr = getDistanceSqr(glm::vec2{transform.position},
                                             glm::vec2{ot.position});

          if (distanceSqr <= 1.0f) {
            const int explosionLight = takeProjectileLight(projectile.entity);
            const int otherExplosionLight =
                takeProjectileLight(otherProjectile->entity);
            explodeAtProjectile(explosionLight, otherExplosionLight);

            destroyProjectileWithEffects(closeEntity);
            destroyProjectileWithEffects(projectile.entity);

            return ExecuteOnNearbyCellsStatus::Done;
          }
        }
      }

      return ExecuteOnNearbyCellsStatus::Running;
    });
  }
}
