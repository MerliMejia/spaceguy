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

        renderGraph.context.particlesData[from].dir = glm::vec3(toCenterDir);
        renderGraph.context.particlesData[from].speed = randomSpeed;
        renderGraph.context.particlesData[from].outerRadius = 1.0f;

        renderGraph.context.particlesData[from].lifeTime = 100.0f;

        currentSpherePoint++;
      }
    } else if (emitter.shape == ParticleEmitterShape::Star) {

      // "Star" means that it throws particles from the center to n, s, e, w, ne, nw, se, sw
      std::array<glm::vec3, 8> directions{
          glm::vec3(0, 0, 1),   // n
          glm::vec3(0, 0, -1),  // s
          glm::vec3(1, 0, 0),   // e
          glm::vec3(-1, 0, 0),  // w
          glm::vec3(1, 0, 1),   // ne
          glm::vec3(-1, 0, 1),  // nw
          glm::vec3(1, 0, -1),  // se
          glm::vec3(-1, 0, -1), // sw
      };

      size_t dirIndex = 0;
      float speed = 0.0f;

      for (size_t to = from + emitter.maxParticles; from < to; from++) {
        if (from > MAX_PARTICLES) {
          break;
        }

        if (dirIndex > 7) {
          dirIndex = 0;
        }

        glm::vec3 nDir = glm::normalize(directions[dirIndex]);

        renderGraph.context.particlesData[from].position = nDir * 0.4f; // local position

        renderGraph.context.particlesData[from].dir = nDir;
        renderGraph.context.particlesData[from].speed = speed;

        dirIndex++;
        speed += 0.5f;

        renderGraph.context.particlesData[from].lifeTime = 100.0f;
      }
    }
  }
}
void updateParticlesSystem() {}
