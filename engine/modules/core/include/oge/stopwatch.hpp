#pragma once
#include <chrono>
#include <cinttypes>

namespace oge
{
class Stopwatch
{
   public:
    using clock = std::chrono::steady_clock;

    static Stopwatch Start();
    float Restart();

   private:
    explicit Stopwatch(clock::time_point start);

    clock::time_point m_start;
};
}  // namespace oge