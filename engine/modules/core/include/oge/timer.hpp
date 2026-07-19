#pragma once
#include <chrono>

namespace oge
{
using timestamp = std::chrono::steady_clock::time_point;

class Timer
{
   public:
    Timer() : m_last(std::chrono::steady_clock::now()) {}

    float Tick()
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> delta = now - m_last;
        m_last = now;
        return delta.count();
    }

   private:
    std::chrono::steady_clock::time_point m_last;
};

}  // namespace OneGame::Engine