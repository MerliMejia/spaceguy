#include "resourceManagementSystem.h"
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static std::unordered_map<int, int> entityToRenderables;
static int nextEntityId = 1;

static std::unordered_set<int> alive;
static std::vector<Renderable> renderables;

static std::vector<int> destroyQueue;

int createEntity() {
  int next = nextEntityId++;
  alive.insert(next);

  return next;
}

Renderable &addRenderable(int entity) {
  if (!alive.contains(entity)) {
    throw std::runtime_error("This entity doesn't live anymore");
  }

  auto it = entityToRenderables.find(entity);
  if (it != entityToRenderables.end()) {
    throw std::runtime_error("This entity already has a renderable");
  }

  int renderableIndex = renderables.size();
  renderables.push_back(Renderable{.entity = entity});
  entityToRenderables[entity] = renderableIndex;

  return renderables[renderableIndex];
}

Renderable &getRenderable(int entity) {
  auto r = entityToRenderables.find(entity);

  if (r != entityToRenderables.end()) {
    return renderables[r->second];
  }

  throw std::runtime_error("Entity doesn't have a renderable");
}

void destroyEntity(int entity) { destroyQueue.push_back(entity); }

static void destroyRenderable(int entity) {
  auto it = entityToRenderables.find(entity);
  if (it == entityToRenderables.end()) {
    return;
  }
  int rIndexToRemove = it->second;
  int rLastIndex = renderables.size() - 1;

  if (rIndexToRemove != rLastIndex) {
    Renderable moved = renderables[rLastIndex];

    renderables[rIndexToRemove] = moved;
    entityToRenderables[moved.entity] = rIndexToRemove;
  }

  renderables.pop_back();
  entityToRenderables.erase(entity);
}

void processDestroyQueue() {
  for (int entity : destroyQueue) {
    if (!alive.contains(entity)) {
      continue;
    }
    destroyRenderable(entity);
    alive.erase(entity);
  }

  destroyQueue.clear();
}
