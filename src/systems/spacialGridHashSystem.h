#pragma once
#include <functional>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

struct CellCoord {
  int x;
  int y;

  bool operator==(const CellCoord &other) const {
    return x == other.x && y == other.y;
  }
};

struct CellHash {
  size_t operator()(const CellCoord &cell) const {
    return std::hash<int>{}(cell.x) ^ (std::hash<int>{}(cell.y) << 1);
  }
};

using SpacialGrid = std::unordered_map<CellCoord, std::vector<int>, CellHash>;

struct SpacialGridContext {
  int rows = 30;
  int columns = 30;
  float cellWidth = 0.0f;
  float cellHeight = 0.0f;
  SpacialGrid grid;
};

extern SpacialGridContext spacialGridContext;

CellCoord worldToCell(const glm::vec2 &position, float cellWidth,
                      float cellHeight);

enum class ExecuteOnNearbyCellsStatus { Done, Running };

void executeOnNearbyCells(
    const CellCoord &center,
    std::function<ExecuteOnNearbyCellsStatus(int)> execute, int square = 1);

void initSpacialGridHash();
void updateSpacialGridHash();
