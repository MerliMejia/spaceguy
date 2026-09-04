#pragma once

#include <stdexcept>
#include <vector>

namespace Renderer {
namespace Shaders {
struct BankManager {
  constexpr static int MODELS_FROM = 0;
  constexpr static int MAX_MODELS = 20;

  int mat4Index = MAX_MODELS;

  std::vector<int> freeModelIndices;

  void init() {
    freeModelIndices.reserve(MAX_MODELS);
    for (int i = MODELS_FROM; i < MAX_MODELS; i++) {
      freeModelIndices.push_back(i);
    }
  }

  int useModelSlot() {
    if (freeModelIndices.empty()) {
      throw std::runtime_error("No model indices available");
    }

    int modelIndex = freeModelIndices.back();
    freeModelIndices.pop_back();

    return modelIndex;
  }
  void releaseModelSlot(int index) { freeModelIndices.push_back(index); }
};

extern BankManager bankManager;
} // namespace Shaders
} // namespace Renderer
