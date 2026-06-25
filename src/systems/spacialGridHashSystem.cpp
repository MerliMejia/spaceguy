#include "spacialGridHashSystem.h"
#include "../engine/vulkanRenderer.h"
#include "../utils/math.h"
#include "../utils/time.h"
#include "glm/fwd.hpp"
#include "resourceManagementSystem.h"
#include <algorithm>
#include <chrono>

SpacialGridContext spacialGridContext{};
// 30 is the scalled floor size
static float floorHalfSize = 30.0f;
static float floorSize = floorHalfSize * 2.0f;
static glm::vec4 gridColor{0.25f, 0.25f, 0.25f, 1.0f};
static glm::vec4 occupiedCellColor{0.0f, 1.0f, 1.0f, 1.0f};
static glm::vec4 nearbyQueryColor{1.0f, 1.0f, 0.0f, 1.0f};
static constexpr float nearbyQueryDebugDuration = 2.0f;

struct TimedDebugCell {
  CellCoord cell{};
  float timeLeft = 0.0f;
};

static std::vector<TimedDebugCell> nearbyQueryDebugCells;

CellCoord worldToCell(const glm::vec2 &position, float cellWidth,
                      float cellHeight) {
  return {
      static_cast<int>(std::floor((position.x + floorHalfSize) / cellWidth)),
      static_cast<int>(std::floor((position.y + floorHalfSize) / cellHeight)),
  };
}

static bool isCellInBounds(const CellCoord &cell) {
  return cell.x >= 0 && cell.x < spacialGridContext.rows && cell.y >= 0 &&
         cell.y < spacialGridContext.columns;
}

static glm::vec3 cellToOrigin(const CellCoord &cell, float z) {
  return glm::vec3{
      -floorHalfSize +
          static_cast<float>(cell.x) * spacialGridContext.cellWidth,
      -floorHalfSize +
          static_cast<float>(cell.y) * spacialGridContext.cellHeight,
      z,
  };
}

static void rememberNearbyQueryCell(const CellCoord &cell) {
  for (TimedDebugCell &debugCell : nearbyQueryDebugCells) {
    if (debugCell.cell == cell) {
      debugCell.timeLeft = nearbyQueryDebugDuration;
      return;
    }
  }

  nearbyQueryDebugCells.push_back(TimedDebugCell{
      .cell = cell,
      .timeLeft = nearbyQueryDebugDuration,
  });
}

void executeOnNearbyCells(
    const CellCoord &center,
    std::function<ExecuteOnNearbyCellsStatus(int)> execute) {
  for (int y = center.y - 1; y <= center.y + 1; y++) {
    bool shouldBreak = false;

    for (int x = center.x - 1; x <= center.x + 1; x++) {
      CellCoord cell{x, y};

      if (vulkanRendererContext.isDebug && isCellInBounds(cell)) {
        rememberNearbyQueryCell(cell);
      }

      auto it = spacialGridContext.grid.find(cell);
      if (it == spacialGridContext.grid.end()) {
        continue;
      }

      for (int entity : it->second) {
        ExecuteOnNearbyCellsStatus status = execute(entity);
        if (status == ExecuteOnNearbyCellsStatus::Done) {
          shouldBreak = true;
          break;
        }
      }
    }

    if (shouldBreak) {
      break;
    }
  }
}

static void buildSpacialGridHash() {
  spacialGridContext.cellWidth = floorSize / spacialGridContext.rows;
  spacialGridContext.cellHeight = floorSize / spacialGridContext.columns;
  spacialGridContext.grid.clear();

  for (WizardBehaviorComponent &wizard : resources.wizardBehaviors) {
    TransformComponent &wtc = getTransform(wizard.entity);
    const Transform &wt = modelToTransform(wtc.model);
    glm::vec2 wizardPosition = glm::vec2{wt.position};

    CellCoord cellCoord =
        worldToCell(wizardPosition, spacialGridContext.cellWidth,
                    spacialGridContext.cellHeight);

    spacialGridContext.grid[cellCoord].push_back(wizard.entity);
  }
}

void initSpacialGridHash() { spacialGridContext.grid.reserve(256); }

void updateSpacialGridHash() {
  auto start = std::chrono::steady_clock::now();

  buildSpacialGridHash();

  auto end = std::chrono::steady_clock::now();
  uint64_t elapsedUs =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start)
          .count();

  uint64_t insertedEntities = 0;
  uint64_t maxCellPopulation = 0;
  for (const auto &[cell, entities] : spacialGridContext.grid) {
    uint64_t cellPopulation = static_cast<uint64_t>(entities.size());
    insertedEntities += cellPopulation;
    maxCellPopulation = std::max(maxCellPopulation, cellPopulation);
  }

  if (!vulkanRendererContext.isDebug) {
    nearbyQueryDebugCells.clear();
    return;
  }

  addDebugGridCellsXY(glm::vec3{-floorHalfSize, -floorHalfSize, 1.0f},
                      spacialGridContext.rows, spacialGridContext.columns,
                      spacialGridContext.cellWidth,
                      spacialGridContext.cellHeight, gridColor);

  for (const auto &[cell, entities] : spacialGridContext.grid) {
    if (!isCellInBounds(cell)) {
      continue;
    }

    addDebugCellXY(cellToOrigin(cell, 1.01f), spacialGridContext.cellWidth,
                   spacialGridContext.cellHeight, occupiedCellColor);
  }

  for (TimedDebugCell &debugCell : nearbyQueryDebugCells) {
    debugCell.timeLeft -= timeState.deltaTime;

    if (debugCell.timeLeft <= 0.0f || !isCellInBounds(debugCell.cell)) {
      continue;
    }

    addDebugCellXY(cellToOrigin(debugCell.cell, 1.02f),
                   spacialGridContext.cellWidth, spacialGridContext.cellHeight,
                   nearbyQueryColor);
  }

  nearbyQueryDebugCells.erase(
      std::remove_if(nearbyQueryDebugCells.begin(), nearbyQueryDebugCells.end(),
                     [](const TimedDebugCell &debugCell) {
                       return debugCell.timeLeft <= 0.0f ||
                              !isCellInBounds(debugCell.cell);
                     }),
      nearbyQueryDebugCells.end());
}
