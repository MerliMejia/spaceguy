#pragma once

#include <glm/glm.hpp>

void initializeProjectiles();
void spawnWizardProjectile(int entity);
void spawnWizardProjectile(int entity, glm::vec3 direction);
void updateProjectiles();
