struct Particle {
  float3 position;
  float speed;
  float3 dir;
  float maxDistance;
  float distance;
  uint isTooFar;
  float2 _pad1;
};
