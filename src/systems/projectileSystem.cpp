#include "projectileSystem.h"
#include "../engine/blender/importer.h"
#include "../utils/generators.h"
#include "../utils/math.h"
#include "../utils/time.h"
#include "../utils/types.h"
#include "glm/fwd.hpp"
#include "particleLifeSystem.h"
#include "resourceManagementSystem.h"
#include "spacialGridHashSystem.h"
#include <unordered_set>

static Mesh wizardProjectileMesh;
std::unordered_set<int> projectileHits;

const constexpr float timeToDie = 4.0f;

void initializeProjectiles() {
  BlenderModel wizardProjectile = loadModel("assets/Wizard_Projectile.3d");
  wizardProjectileMesh =
      generateMesh(wizardProjectile.vertices, wizardProjectile.indices);
}

void spawnWizardProjectile(int wizard) {
  TransformComponent tc = getTransform(wizard);
  Transform wizardTransform = modelToTransform(tc.model);

  glm::vec3 localForward{0.0f, -1.0f, 0.0f};
  glm::vec3 forward = glm::normalize(wizardTransform.rotation * localForward);

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

  // At some point should be a constant
  emitter.lifeColorStart = {1.0f, 0.0f, 0.0f, 1.0f};
  emitter.lifeColorEnd = {0.0f, 0.0f, 1.0f, 1.0f};

  emitter.speedColorSlow = {1.0f, 0.0f, 0.0f, 1.0f};
  emitter.speedColorFast = {1.0f, 0.0f, 0.0f, 1.0f};

  emitter.particleStartSize = 0.5;
  emitter.particleEndSize = 0.01;
  emitter.entity = travelEffectEmitterEntity;

  emitter.owner = projectile;
  projectileComponent.travelEffectEmitter = travelEffectEmitterEntity;
}

void updateProjectiles() {
  for (ProjectileComponent &projectile : resources.projectiles) {

    if (projectile.timeAlive >= timeToDie) {
      addParticleEmitterToBeDestroyed(projectile.travelEffectEmitter);
      destroyEntity(projectile.entity);
      continue;
    }

    projectile.timeAlive += timeState.deltaTime;

    TransformComponent &tc = getTransform(projectile.entity);
    Transform transform = modelToTransform(tc.model);

    transform.position +=
        projectile.direction * projectile.speed * timeState.deltaTime;

    tc.model = transformToModel(transform.position, transform.rotation,
                                transform.scale);

    ParticleEmitterCpuComponent &travelEmitter =
        getParticleEmitterCpuComponent(projectile.travelEffectEmitter);

    travelEmitter.position = transform.position;
    travelEmitter.direction = projectile.direction;

    // Check hits
    CellCoord center =
        worldToCell(transform.position, spacialGridContext.cellWidth,
                    spacialGridContext.cellHeight);

    executeOnNearbyCells(center, [transform, projectile](int closeEntity) {
      auto explodeAtProjectile = [&]() {
        int explosionEffect = createEntity();

        ParticleEmitterCpuComponent &emitter =
            addParticleEmitterCpuComponent(explosionEffect);

        generateExplosionParticleEmitter(emitter, transform.position, 2);

        emitter.lifeColorStart = {1.0f, 0.0f, 0.0f, 1.0f};
        emitter.lifeColorEnd = {0.0f, 0.0f, 1.0f, 1.0f};

        emitter.speedColorSlow = {1.0f, 0.0f, 0.0f, 1.0f};
        emitter.speedColorFast = {1.0f, 0.0f, 0.0f, 1.0f};
        emitter.entity = explosionEffect;

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
            explodeAtProjectile();

            addParticleEmitterToBeDestroyed(projectile.travelEffectEmitter);

            destroyEntity(wizard->shootEffecEntity);
            destroyEntity(closeEntity);
            destroyEntity(projectile.entity);

            return ExecuteOnNearbyCellsStatus::Done;
          }
        }
      }

      // Projectiles
      if (ProjectileComponent *otherProjectile =
              tryGetProjectile(closeEntity)) {
        if (otherProjectile->entity != projectile.entity &&
            otherProjectile->ownerEntity != projectile.ownerEntity) {
          TransformComponent &otc = getTransform(closeEntity);
          const Transform &ot = modelToTransform(otc.model);

          float distanceSqr = getDistanceSqr(glm::vec2{transform.position},
                                             glm::vec2{ot.position});

          if (distanceSqr <= 1.0f) {
            explodeAtProjectile();

            addParticleEmitterToBeDestroyed(projectile.travelEffectEmitter);
            addParticleEmitterToBeDestroyed(
                otherProjectile->travelEffectEmitter);

            destroyEntity(closeEntity);
            destroyEntity(projectile.entity);

            return ExecuteOnNearbyCellsStatus::Done;
          }
        }
      }

      return ExecuteOnNearbyCellsStatus::Running;
    });
  }
}
