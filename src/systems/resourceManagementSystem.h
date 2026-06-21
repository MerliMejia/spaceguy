#pragma once

#include "../engine/vulkanRenderer.h"
#include "../utils/types.h"

// enum class ObjectRenderKind { Static, Animated, TransformAnimated };

struct Renderable {
  int entity = -1;
  ObjectRenderKind renderKind = ObjectRenderKind::Static;
  const Mesh *mesh = nullptr;
  const AnimatedMesh *animatedMesh = nullptr;
  const TransformAnimatedMesh *transformAnimatedMesh = nullptr;
};

struct Resources {
  std::vector<Renderable> renderables;
};

extern Resources resources;

int createEntity();
Renderable &addRenderable(int entity);
Renderable &getRenderable(int entity);

void destroyEntity(int entity);
void processDestroyQueue();
