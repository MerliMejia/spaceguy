#pragma once
#include "../../behaviors/decisionTree.h"
constexpr int HOW_CLOSE = 16;

DecisionStatus startAttackModeLogic(int entity);
bool isAlreadyInAttackMode(int entity);
DecisionStatus iddleModeLogic(int entiy);
bool checkIfSomeoneCloseLogic(int entity);
DecisionStatus attackModeLogic(int entity);
bool waitedForInitAttackLogic(int entity);
DecisionStatus moveToAttackLogic(int entity);
bool isCloseEnoughToAttackTarget(int entity);
DecisionStatus flyingAttackLogic(int entity);
bool isAttackEntityIsValid(int entity);
