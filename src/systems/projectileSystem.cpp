#include "projectileSystem.h"
#include "../engine/blender/importer.h"
#include "../utils/generators.h"
#include "../utils/math.h"
#include "../utils/time.h"
#include "../utils/types.h"
#include "glm/fwd.hpp"
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

  Renderable &renderable = addComponent<Renderable>(projectile);
  renderable.mesh = &wizardProjectileMesh;
  renderable.renderKind = ObjectRenderKind::Static;

  TransformComponent &ptc = addTransform(projectile);
  Transform pt = modelToTransform(ptc.model);
  pt.position = initialPos;
  pt.scale = glm::vec3{0.5f};
  ptc.model = transformToModel(pt.position, pt.rotation, pt.scale);

  ProjectileComponent &projectileComponent = addProjectile(projectile);
  projectileComponent.direction = forward;
  projectileComponent.ownerEntity = wizard;
}

void updateProjectiles() {
  for (ProjectileComponent &projectile : resources.projectiles) {

    if (projectile.timeAlive >= timeToDie) {
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

    // Check hits
    CellCoord center =
        worldToCell(transform.position, spacialGridContext.cellWidth,
                    spacialGridContext.cellHeight);

    executeOnNearbyCells(center, [transform, projectile](int closeEntity) {
      // Wizards
      if (WizardBehaviorComponent *wizard = tryGetWizardBehavior(closeEntity)) {

        if (wizard->entity != projectile.ownerEntity) {
          TransformComponent &wtc = getTransform(closeEntity);
          const Transform &wt = modelToTransform(wtc.model);

          float distance = getDistanceSqr(glm::vec2{transform.position},
                                          glm::vec2{wt.position});
          float distanceSQ = distance * distance;

          if (distanceSQ <= 1) {
            destroyEntity(wizard->shootEffecEntity);
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
