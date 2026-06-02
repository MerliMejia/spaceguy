#pragma once
#include <chrono>

struct TimeState
{
    using Clock = std::chrono::steady_clock;

    Clock::time_point previousTime = Clock::now();

    float currentTime = 0.0f;
    float deltaTime = 0.0f;
};

extern TimeState timeState;

void updateTime();