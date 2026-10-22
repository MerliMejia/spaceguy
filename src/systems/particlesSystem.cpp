#include "particlesSystem.h"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "resourceManagementSystem.h"
#include "utils/math.h"
#include "utils/types.h"
#include <array>
#include <cstddef>

void initParticlesSystem(Renderer::RenderGraph::RenderGraph &renderGraph) {
  size_t from = 0;

  for (auto &emitter : resources.particleEmitters) {
    TransformComponent &tc = getTransform(emitter.entity);
    Transform t = modelToTransform(tc.model);

    emitter.indexOffset = from;

    if (emitter.shape == ParticleEmitterShape::Cone) {

      std::vector<glm::vec2> outCircles = generateCirclePoints(t.position, 1.0f, emitter.maxParticles);

      size_t currentCirclePoint = 0;

      for (size_t to = from + emitter.maxParticles; from < to; from++) {
        if (from > MAX_PARTICLES) {
          break;
        }

        float randomSpeed = randomFloat(5.0f);

        glm::vec3 outCirclePosition = glm::vec3(outCircles[currentCirclePoint], 0.0);

        renderGraph.context.particlesData[from].position = outCirclePosition;

        glm::vec3 radial = outCirclePosition - glm::vec3(0.0f);
        glm::vec3 forward = -glm::normalize(emitter.direction);

        // At some point convert these magic numbers into configs...
        renderGraph.context.particlesData[from].dir = glm::normalize(forward * 1.0f + glm::normalize(radial) * 1.0f);
        renderGraph.context.particlesData[from].speed = randomSpeed;

        currentCirclePoint++;
      }
    }
  }
}
void updateParticlesSystem() {}
