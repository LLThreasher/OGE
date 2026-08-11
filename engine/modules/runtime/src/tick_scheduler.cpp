#include "oge/runtime/tick_scheduler.hpp"

#include <cassert>
#include <thread>

namespace oge::runtime
{
TickScheduler::TickScheduler(float fixedDelta)
    : m_fixedDelta(fixedDelta)
{
}

bool TickScheduler::Poll(float dt)
{
    m_accumulator += dt;
    return m_accumulator >= m_fixedDelta;
}

float TickScheduler::ConsumeTick()
{
    if (m_accumulator >= m_fixedDelta)
    {
        m_accumulator -= m_fixedDelta;
        return m_fixedDelta;
    }
    return 0.0f;
}

float TickScheduler::GetAlpha() const
{
    return m_accumulator / m_fixedDelta;
}

void TickScheduler::SetInterval(float interval)
{
    assert(interval > 0.f);
    m_fixedDelta = interval;
}

BlockingTickScheduler::BlockingTickScheduler(float interval)
    : m_tickInterval(std::chrono::duration_cast<clock::duration>(
          std::chrono::duration<double>(interval)))
{
    m_nextTick = clock::now();
    m_lastTick = m_nextTick;
}

double BlockingTickScheduler::WaitForNextTick()
{
    // Capture the ideal target for this tick before advancing.
    // We sleep until `target`, not `m_nextTick`, so the schedule
    // stays aligned to target + N*interval regardless of oversleep.
    auto target = m_nextTick;
    m_nextTick = target + m_tickInterval;

    auto now = clock::now();
    if (now < target)
    {
        std::this_thread::sleep_until(target);
        now = clock::now();
    }

    // If we fell more than a full interval behind schedule (system
    // suspend, heavy frame), reset to now.  Otherwise we'd burst
    // through a backlog of zero-duration ticks.
    if (now > m_nextTick)
    {
        m_nextTick = now + m_tickInterval;
    }

    // Return the actual time since the last tick, not the ideal
    // interval.  Using the ideal interval when sleep_until overslept
    // (or the last frame ran long) causes simulation timer drift.
    double dt = std::chrono::duration<double>(now - m_lastTick).count();
    m_lastTick = now;
    return dt;
}
}  // namespace oge::runtime
