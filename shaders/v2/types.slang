struct Particle {
  float3 position;
  float speed;
  float3 dir;
  float maxDistance;
  float distance;
  uint isTooFar;
  uint isAlive;
  float _pad1;
};
