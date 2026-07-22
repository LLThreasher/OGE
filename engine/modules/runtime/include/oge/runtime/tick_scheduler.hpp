#pragma once

#include <algorithm>
#include <chrono>
#include <thread>

#include "oge/log.hpp"

namespace oge::runtime
{
class TickScheduler
{
   public:
    explicit TickScheduler(float interval = 1 / 60.0f);

    // Returns true if a tick is ready.
    bool Poll(float dt);

    // Call repeatedly after Poll() while this returns > 0
    float ConsumeTick();

    float GetAlpha() const;

   private:
    float m_fixedDelta;
    float m_targetFrameTime;

    float m_accumulator = 0.0f;

    const float m_maxFrameTime = 0.25f;
};

class BlockingTickScheduler
{
   public:
    explicit BlockingTickScheduler(double tickRate);

    double WaitForNextTick();

   private:
    using clock = std::chrono::steady_clock;

    clock::duration m_tickInterval;
    clock::time_point m_nextTick;
};
} // namespace oge::runtime
