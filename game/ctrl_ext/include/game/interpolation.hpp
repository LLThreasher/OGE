#pragma once

#include <cmath>
#include <deque>
#include <unordered_map>

#include "game/components.hpp"
#include "game/game_world.hpp"
#include "game/sim/player_sim_config.hpp"
#include "oge/math.hpp"

namespace game
{

// =========================================================================
// InterpolationLayer
//
// Presentation-layer helper that computes non-authoritative interpolated
// transforms for remote copies — entities tagged
// RenderStrategyTag<Interpolation> (Phase 4 two-world contract).
//
//   PreUpdate(authoritativeWorld)   — snapshot the authoritative mirror's
//     ComponentPhysicBody per Interpolation-tagged entity into a per-entity
//     history with arrival ticks.
//   PostUpdate(renderWorld, alpha)  — write ComponentInterpolatedTransform
//     in the render world via an exponential attractor toward the
//     EXTRAPOLATED authoritative state: target = latest snapshot position +
//     velocity x render delay, where the render delay is the age of the
//     latest applied fixed-tick state (alpha x kFixedFrameDuration).  The
//     attractor rate k = 12/s spreads a correction snap over ~0.3 s instead
//     of teleporting the copy.
//
// Local player exempt: RenderStrategyTag<LocalPrediction> entities are
// skipped — the prediction-world body IS their render state (the realtime
// sim wrote it), so they never pass through this buffer.
//
// Usage (in SceneView::Update):
//   m_interp.PreUpdate(scene.GetAuthoritativeWorld());
//   scene.Update(frame, sctx);      // physics tick may advance
//   m_interp.PostUpdate(world, alpha, f.dt);
//   renderers.Update(...);          // read ComponentInterpolatedTransform
// =========================================================================

class InterpolationLayer
{
   public:
    // Exponential attractor rate: pos += (target - pos) * (1 - e^(-k*dt)).
    // 12/s means a correction converges to ~2% of its size in 20 render
    // frames (~0.33 s at 60 Hz).
    static constexpr float kAttractorRate = 12.f;

    // Snapshot the authoritative mirror for Interpolation-tagged entities.
    // Call BEFORE Scene::Update (before the fixed tick may advance).
    void PreUpdate(const GameWorld& authoritative)
    {
        ++m_arrivalTick;

        // Prune entities that left the authoritative world or lost the tag.
        for (auto it = m_snapshots.begin(); it != m_snapshots.end();)
        {
            if (!authoritative.valid(it->first) ||
                !authoritative.all_of<
                    RenderStrategyTag<RenderStrategy::Interpolation>>(
                    it->first))
            {
                it = m_snapshots.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto [e, body] :
             authoritative
                 .view<RenderStrategyTag<RenderStrategy::Interpolation>,
                       const ComponentPhysicBody>()
                 .each())
        {
            auto& history = m_snapshots[e];
            history.push_back(Snapshot{body.pos, body.velocity, m_arrivalTick});
            // Bound the history — only the latest snapshot drives the
            // target today; keep the rest for future buffer-delay work.
            while (history.size() > kMaxHistory)
            {
                history.pop_front();
            }
        }
    }

    // Write ComponentInterpolatedTransform for Interpolation-tagged remote
    // copies.  Call AFTER Scene::Update with the scheduler's alpha and the
    // render frame's dt.
    void PostUpdate(GameWorld& renderWorld, float alpha,
                    float dt = 1.f / 60.f)
    {
        alpha = math::clamp(alpha, 0.f, 1.f);
        const float factor = 1.f - std::exp(-kAttractorRate * dt);

        // Prune attractor state for entities that left the render world.
        for (auto it = m_interpPositions.begin();
             it != m_interpPositions.end();)
        {
            if (!renderWorld.valid(it->first))
            {
                it = m_interpPositions.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto [e, body] :
             renderWorld
                 .view<RenderStrategyTag<RenderStrategy::Interpolation>,
                       const ComponentPhysicBody>()
                 .each())
        {
            // Local player exempt (D9): the prediction-world body is the
            // local render state — no buffer, no added delay.
            if (renderWorld.all_of<
                    RenderStrategyTag<RenderStrategy::LocalPrediction>>(e))
            {
                continue;
            }

            auto snapIt = m_snapshots.find(e);
            if (snapIt == m_snapshots.end() || snapIt->second.empty())
            {
                continue;  // nothing authoritative yet — wait for a snapshot
            }

            const Snapshot& latest = snapIt->second.back();
            // Extrapolate the latest applied fixed-tick state forward to
            // the current render time: alpha measures the age of that
            // state within the tick interval.
            const float renderDelay = alpha * sim::kFixedFrameDuration;
            const math::vec3 target =
                latest.pos + latest.velocity * renderDelay;

            auto posIt = m_interpPositions.find(e);
            if (posIt == m_interpPositions.end())
            {
                // Seed from the render body so the first frames don't fly
                // across the map toward the authoritative state.
                posIt = m_interpPositions.emplace(e, body.pos).first;
            }

            posIt->second += (target - posIt->second) * factor;
            renderWorld.emplace_or_replace<ComponentInterpolatedTransform>(
                e, posIt->second);
        }
    }

   private:
    struct Snapshot
    {
        math::vec3 pos{};
        math::vec3 velocity{};
        uint32_t arrivalTick = 0;
    };

    static constexpr size_t kMaxHistory = 16;

    std::unordered_map<entt::entity, std::deque<Snapshot>> m_snapshots;
    std::unordered_map<entt::entity, math::vec3> m_interpPositions;
    uint32_t m_arrivalTick = 0;
};

}  // namespace game
