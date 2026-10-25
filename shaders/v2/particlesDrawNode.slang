import banks;
import types;
#include "banks_shared.h"

struct VSInput {
  float3 inPosition;
  float2 uv;
  float3 normal;
  uint instanceId : SV_InstanceID;
};

struct VSOutput {
  float4 pos : SV_Position;
  float2 uv;
  uint particleTooFar;
  uint particleIsAlive;
  float distanceDiff;
};

// Rotates a vertex given in the mesh's local space so that the mesh's
// local +Z axis points from the particle toward `target`.
// All three inputs must be in the same space (e.g. emitter-local).
float3 faceTarget(float3 localVertex, float3 particlePosition, float3 target) {
  float3 toTarget = target - particlePosition;
  toTarget.z = 0.0f;
  float distanceToTarget = length(toTarget);

  // Particle sits on the target: there is no direction, so keep the
  // mesh unrotated instead of normalizing a zero vector (NaN).
  if (distanceToTarget < 1e-5f)
    return localVertex;

  // Local +Y: the flattened direction toward the target (no tilt).
  float3 towardTarget = toTarget / distanceToTarget;

  // Local +Z: world up. It never changes, so the mesh only spins around Z.
  float3 worldUp = float3(0.0f, 0.0f, 1.0f);

  // Local +X: sideways, perpendicular to both.
  // Y x Z = X keeps the basis right-handed (the mesh isn't mirrored).
  // Both inputs are unit length and perpendicular, so no normalize is needed.
  float3 sideways = cross(towardTarget, worldUp);

  // Rotation = "walk x along sideways, y toward the target, z straight up".
  return sideways * localVertex.x + towardTarget * localVertex.y + worldUp * localVertex.z;
}

[shader("vertex")]
VSOutput vertMain(VSInput input) {
  uint modelIndex = PushConstantsBank::getInt(SG_PUSH_MODEL_INDEX);
  int particleEmitterIndex = PushConstantsBank::getInt(SG_PUSH_PARTICLE_EMITTER_INDEX);

  float4x4 model = UniformBank::getFloat4x4(modelIndex);
  float4x4 view = UniformBank::getFloat4x4(SG_VIEW_INDEX);
  float4x4 proj = UniformBank::getFloat4x4(SG_PROJ_INDEX);
  Particle currentParticle = particles[particleEmitterIndex + input.instanceId];

  // In emitter-local space the emitter's origin is simply (0, 0, 0).
  float3 emitterOrigin = float3(0, 0, 0);

  // 1. Rotate the vertex around the mesh's own origin.
  float3 rotatedVertex = faceTarget(input.inPosition, currentParticle.position, emitterOrigin);

  // 2. Move it to where the particle is (still emitter-local).
  float3 emitterLocal = rotatedVertex + currentParticle.position;

  // 3. Emitter-local -> world -> view -> clip.
  float4 world = mul(model, float4(emitterLocal, 1.0f));

  VSOutput out;
  out.pos = mul(proj, mul(view, world));
  out.uv = input.uv;
  out.particleTooFar = currentParticle.isTooFar;
  out.particleIsAlive = currentParticle.isAlive;
  out.distanceDiff = (currentParticle.maxDistance - currentParticle.distance) / currentParticle.maxDistance;

  return out;
}

[shader("fragment")]
float4 fragMain(VSOutput input) {

  if (input.particleTooFar != 0) {
    discard;
  }
  if (input.particleIsAlive == 0) {
    discard;
  }

  // uv runs 0..1 across the quad, so (0.5, 0.5) is the center and
  // 0.5 is the radius of the largest circle that fits inside it.
  // float distanceFromCenter = distance(input.uv, float2(0.5f, 0.5f));

  // if (distanceFromCenter > 0.5f) {
  //   discard;
  // }

  int diffTexIndex = PushConstantsBank::getInt(SG_PUSH_DIFF_TEX_INDEX);
  Texture2D<float4> diffuseTexture = textures[diffTexIndex];
  float2 sampledUv = float2(input.uv.x, 1.0f - input.uv.y);
  float4 sampledColor = diffuseTexture.Sample(textureSampler, sampledUv);

  if (sampledColor.a < 0.5f) {
    discard;
  }

  return float4(sampledColor.xyz, clamp(input.distanceDiff, 0.0f, 0.5f));
}
