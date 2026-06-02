#include "time.h"
#include <iostream>

TimeState timeState{};

void updateTime()
{
    TimeState::Clock::time_point currentTime = TimeState::Clock::now();

    timeState.deltaTime =
        std::chrono::duration<float>(currentTime - timeState.previousTime).count();

    timeState.currentTime =
        std::chrono::duration<float>(currentTime.time_since_epoch()).count();

    timeState.previousTime = currentTime;

    // std::cout << "Delta: " << timeState.deltaTime << "Sec\n\n";
}