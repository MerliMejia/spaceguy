#include "behaviors.h"
#include "../systems/resourceManagementSystem.h"
#include "../systems/spacialGridHashSystem.h"
#include "math.h"

BehaviorUtil::IsSomeOneCloseReturn
BehaviorUtil::isSomeoneCloseLogic(int entity, CheckType type, int square,
                                  float checkRadius) {
  TransformComponent &wtc = getTransform(entity);
  const Transform &wt = modelToTransform(wtc.model);
  glm::vec2 currentPos = glm::vec2{wt.position};

  CellCoord cell = worldToCell(currentPos, spacialGridContext.cellWidth,
                               spacialGridContext.cellHeight);

  IsSomeOneCloseReturn toReturn{};

  executeOnNearbyCells(
      cell,
      [entity, &toReturn, currentPos, type, checkRadius](int checkEntity) {
        TransformComponent cwtc;

        if (type == CheckType::All) {
          WizardBehaviorComponent *checkWizard =
              tryGetWizardBehavior(checkEntity);
          OgreBehaviorComponent *checkOgre = nullptr;

          if (checkWizard == nullptr) {
            checkOgre = tryGetOgreBehaviorComponent(checkEntity);

            if (checkOgre == nullptr) {
              return ExecuteOnNearbyCellsStatus::Running;
            }
          }

          if ((checkWizard != nullptr && entity == checkWizard->entity) ||
              checkOgre != nullptr && entity == checkOgre->entity) {
            return ExecuteOnNearbyCellsStatus::Running;
          }

          cwtc = checkWizard != nullptr ? getTransform(checkWizard->entity)
                                        : getTransform(checkOgre->entity);
        }

        if (type == CheckType::Wizards) {
          WizardBehaviorComponent *checkWizard =
              tryGetWizardBehavior(checkEntity);

          if (checkWizard == nullptr) {
            return ExecuteOnNearbyCellsStatus::Running;
          }

          cwtc = getTransform(checkWizard->entity);
        }

        const Transform &cwt = modelToTransform(cwtc.model);
        glm::vec2 checkcurrentPos = glm::vec2{cwt.position};

        float distance = getDistanceSqr(currentPos, checkcurrentPos);

        float closeRadiusSqr = checkRadius * checkRadius;

        if (distance <= closeRadiusSqr) {
          toReturn.isSomeoneClose = true;
          toReturn.closeEntity = checkEntity;
          return ExecuteOnNearbyCellsStatus::Done;
        }

        return ExecuteOnNearbyCellsStatus::Running;
      },
      square);

  return toReturn;
}
