#include "particlesSystem.h"
#include "resourceManagementSystem.h"
#include "utils/types.h"

void initParticlesSystem(Renderer::RenderGraph::RenderGraph &renderGraph) {
  size_t from = 0;

  for (auto &emitter : resources.particleEmitters) {
    TransformComponent &tc = getTransform(emitter.entity);
    Transform t = modelToTransform(tc.model);

    emitter.indexOffset = from;

    if (emitter.shape == ParticleEmitterShape::Sphere) {

      std::vector<glm::vec3> spherePoints = generatePointsOnSphere(emitter.maxParticles, 10,
                                                                   glm::vec3(0.0f)); // Center around the local origin

      size_t currentSpherePoint = 0;

      for (size_t to = from + emitter.maxParticles; from < to; from++) {
        if (from > MAX_PARTICLES) {
          break;
        }

        float randomSpeed = randomFloat(5.0f);

        renderGraph.context.particlesData[from].position = spherePoints[currentSpherePoint];
        // Point to local origin
        glm::vec3 toCenterDir = -glm::normalize(spherePoints[currentSpherePoint]);

        renderGraph.context.particlesData[from].dir = glm::vec4(toCenterDir, randomSpeed);

        renderGraph.context.particlesData[from].lifeTime = 100.0f;

        currentSpherePoint++;
      }
    } else if (emitter.shape == ParticleEmitterShape::Star) {

      for (size_t to = from + emitter.maxParticles; from < to; from++) {
        if (from > MAX_PARTICLES) {
          break;
        }

        float randomSpeed = randomFloat(5.0f);

        renderGraph.context.particlesData[from].position = glm::vec3(0.6f); // local position + small offset

        renderGraph.context.particlesData[from].dir = glm::vec4(emitter.direction, randomSpeed);

        renderGraph.context.particlesData[from].lifeTime = 100.0f;
      }
    }
  }
}
void updateParticlesSystem() {}
