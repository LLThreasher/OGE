#pragma once

#include <chrono>

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

    void SetInterval(float interval);

   private:
    float m_fixedDelta;
    float m_accumulator = 0.0f;
};

class BlockingTickScheduler
{
   public:
    explicit BlockingTickScheduler(float interval = 1 / 30.f);

    double WaitForNextTick();

   private:
    using clock = std::chrono::steady_clock;

    clock::duration m_tickInterval;
    clock::time_point m_nextTick;
};
}  // namespace oge::runtime
