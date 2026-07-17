#include "oge/stopwatch.hpp"

namespace oge
{
Stopwatch Stopwatch::Start() { return Stopwatch(clock::now()); }

float Stopwatch::Restart()
{
    auto now = clock::now();
    auto elapsed = std::chrono::duration<float, std::milli>(now - m_start).count();
    m_start = now;
    return elapsed;
}

Stopwatch::Stopwatch(clock::time_point start) : m_start(start) {}
}  // namespace oge
