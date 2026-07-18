#pragma once

#include <glm/glm.hpp>

void initializeProjectiles();
void spawnWizardProjectile(int entity, int lightEntity = -1);
void spawnWizardProjectile(int entity, glm::vec3 direction,
                           int lightEntity = -1);
void updateProjectiles();
