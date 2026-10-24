#include "particlesSystem.h"
#include "engine/renderer/renderGraph/renderGraphUtils.h"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "resourceManagementSystem.h"
#include "utils/math.h"
#include "utils/time.h"
#include "utils/types.h"
#include <cstddef>
#include <cstdint>

int32_t usedParticles = 0;
std::vector<Renderer::RenderGraph::Context::ParticleRange> freeToUseParticles;

static void updateEmitter(ParticleEmitterComponent &emitter, Renderer::RenderGraph::RenderGraph &renderGraph) {

  TransformComponent &tc = getTransform(emitter.entity);
  Transform t = modelToTransform(tc.model);

  if (emitter.shape == ParticleEmitterShape::Cone) {

    float radius = 1.0f;
    std::vector<glm::vec2> outCircles = generateCirclePoints(t.position, radius, emitter.burst.count);

    size_t currentCirclePoint = 0;

    size_t first = emitter.range.from + emitter.burst.offset;
    renderGraph.context.particlesDirtyRanges.push_back(Renderer::RenderGraph::Context::ParticleRange{.from = first, .to = first + emitter.burst.count});

    for (size_t i = emitter.range.from + emitter.burst.offset; i < emitter.range.from + emitter.burst.offset + emitter.burst.count; i++) {
      if (i >= MAX_PARTICLES) {
        break;
      }

      float randomSpeed = 2.0f;

      glm::vec3 outCirclePosition = glm::vec3(outCircles[currentCirclePoint], 0.0);
      Renderer::Types::Particle &particle = renderGraph.context.particlesData[i];

      particle.position = outCirclePosition;

      glm::vec3 radial = outCirclePosition - glm::vec3(0.0f);
      glm::vec3 forward = -glm::normalize(emitter.direction);
      float topCone = 0.2f;
      float bottomCone = 0.5f;

      // At some point convert these magic numbers into configs...
      particle.dir = glm::normalize(forward * topCone + glm::normalize(radial) * bottomCone);
      particle.speed = randomSpeed;
      particle.isAlive = 1;
      particle.maxDistance = 5.0f;

      currentCirclePoint++;
    }

    emitter.burst.offset += emitter.burst.count;
  }
}

static void initEmitter(ParticleEmitterComponent &emitter, Renderer::RenderGraph::RenderGraph &renderGraph) {
  uint32_t maxParticles = emitter.maxParticles();

  emitter.range.from = usedParticles;
  emitter.range.to = usedParticles + maxParticles;
  usedParticles += maxParticles;

  updateEmitter(emitter, renderGraph);
}

void initParticlesSystem(Renderer::RenderGraph::RenderGraph &renderGraph) {

  for (auto &emitter : resources.particleEmitters) {
    initEmitter(emitter, renderGraph);
  }
}

void updateParticlesSystem(Renderer::RenderGraph::RenderGraph &renderGraph) {
  for (auto &emitter : resources.particleEmitters) {
    emitter.time += timeState.deltaTime;

    // std::cout << "EMITER TIME: " << emitter.time << std::endl;

    if (emitter.time >= emitter.duration) {
      if (emitter.loop) {
        emitter.time = 0;
        emitter.burst.time = 0;
        emitter.burst.offset = 0;
        updateEmitter(emitter, renderGraph);
      } else {
        // TODO - At some point when particle emitters are able to be spawned in mid game.
        // freeToUseParticles.push_back(emitter.range);
        // destroyEntity(emitter.entity);
        continue;
      }
    }

    emitter.burst.time += timeState.deltaTime;
    if (emitter.burst.time >= emitter.burst.rate) {
      updateEmitter(emitter, renderGraph);
      emitter.burst.time = 0;
    }
  }
}
