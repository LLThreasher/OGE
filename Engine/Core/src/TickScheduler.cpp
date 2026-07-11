#include "Engine/TickScheduler.hpp"

#include <algorithm>
#include <functional>
#include <thread>

TickScheduler::TickScheduler(float fixedDelta)
    : m_fixedDelta(fixedDelta), m_targetFrameTime(fixedDelta), m_lastTime(std::chrono::high_resolution_clock::now())
{
}

bool TickScheduler::Poll()
{
    using clock = std::chrono::high_resolution_clock;

    auto now = clock::now();
    std::chrono::duration<float> elapsed = now - m_lastTime;

    // Sleep if we're running too fast
    if (elapsed.count() < m_targetFrameTime)
    {
        auto remaining = std::chrono::duration<float>(m_targetFrameTime - elapsed.count());

        std::this_thread::sleep_for(remaining);
        now = clock::now();
        elapsed = now - m_lastTime;
    }

    m_lastTime = now;

    float frameDelta = std::min(elapsed.count(), m_maxFrameTime);

    m_accumulator += frameDelta;

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

float TickScheduler::GetAlpha() const { return m_accumulator / m_fixedDelta; }

HeadlessTickScheduler::HeadlessTickScheduler(double tickRate)
    : m_tickInterval(std::chrono::duration_cast<clock::duration>(std::chrono::duration<double>(1.0 / tickRate)))
{
    m_nextTick = clock::now();
}

double HeadlessTickScheduler::WaitForNextTick()
{
    m_nextTick += m_tickInterval;

    auto now = clock::now();
    if (now < m_nextTick)
    {
        std::this_thread::sleep_until(m_nextTick);
    }

    return std::chrono::duration<double>(m_tickInterval).count();
}
