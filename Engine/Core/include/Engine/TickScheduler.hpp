#pragma once

#include <chrono>
#include <thread>
#include <algorithm>

#include "Engine/Logger.hpp"

class TickScheduler
{
public:
    explicit TickScheduler(float tickRate = 60.0f);

    // Blocks until at least one fixed tick is available.
    // Returns true if a tick is ready.
    bool Poll();

    // Call repeatedly after Poll() while this returns > 0
    float ConsumeTick();

    float GetAlpha() const;

private:
    float m_fixedDelta;
    float m_targetFrameTime;

    float m_accumulator = 0.0f;

    const float m_maxFrameTime = 0.25f;

    std::chrono::high_resolution_clock::time_point m_lastTime;
};

class HeadlessTickScheduler
{
public:
    explicit HeadlessTickScheduler(double tickRate);

    double WaitForNextTick();

private:
    using clock = std::chrono::steady_clock;

    clock::duration m_tickInterval;
    clock::time_point m_nextTick;
};