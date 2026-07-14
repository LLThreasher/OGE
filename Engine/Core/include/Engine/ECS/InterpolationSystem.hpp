#pragma once
#include <cinttypes>

#include "Engine/entt.hpp"

namespace OneGame::Engine::ECS
{
    template <typename T, size_t size>
    struct SnapshotBuffer
    {

    };

    template <typename T>
    class Interpolator
    {
        static void Interpolate(T x, T y, float a) = 0;
    };

    constexpr size_t SNAPSHOT_BUFFER_SIZE = 32;

    class InterpolationSystem
    {
    public:
        template <typename T, typename P>
        void Register(entt::registry& game)
        {
            game.on_construct<T>().connect([](auto r, auto e) {
                r.add<SnapshotBuffer<T, SNAPSHOT_BUFFER_SIZE>>(e);
            });
        }
        void SetDelay(float delay);
    private:
        float m_delay = 0.f;
    };
} // namespace OneGame::Engine::ECS
