#pragma once
#include <functional>

#include "Engine/GameAppState.hpp"

class TickScheduler
{
   public:
    explicit TickScheduler(float fixedDelta = 1.0f / 60.0f);

    void Tick(float frameDelta, const std::function<void(float)>& fixedUpdate);

    void Update();

    float GetAlpha() const;

   private:
    float m_fixedDelta;
    float m_accumulator = 0.0f;
    float m_alpha = 0.0f;

    const float m_maxFrameTime = 0.25f;  // clamp to avoid spiral
};
