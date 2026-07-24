#pragma once

namespace BehaviorUtil {
enum class CheckType { Wizards, Ogres, All };
constexpr float CLOSE_RADIUS = 1;

struct IsSomeOneCloseReturn {
  bool isSomeoneClose = false;
  int closeEntity = -1;
};

IsSomeOneCloseReturn isSomeoneCloseLogic(int entity, CheckType type,
                                         int square = 1,
                                         float checkRadius = CLOSE_RADIUS);
} // namespace BehaviorUtil
