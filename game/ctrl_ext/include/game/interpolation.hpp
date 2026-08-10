#pragma once

#include <unordered_map>

#include "game/components.hpp"
#include "game/game_world.hpp"
#include "oge/math.hpp"

namespace game
{

// =========================================================================
// InterpolationLayer
//
// Presentation-layer helper that computes non-authoritative interpolated
// transforms for entities tagged with RenderStrategyTag<Interpolation>.
//
// Usage (in SceneView::Update):
//   m_interp.PreUpdate(world);
//   scene.Update(frame, sctx);     // physics tick may advance
//   m_interp.PostUpdate(world, alpha);
//   renderers.Update(...);         // read ComponentInterpolatedTransform
// =========================================================================

class InterpolationLayer
{
   public:
    // Snapshot current physics positions for Interpolation-tagged entities.
    // Call BEFORE Scene::Update (before the physics tick may advance).
    void PreUpdate(GameWorld& world)
    {
        // Prune stale entries from previous frame.
        auto it = m_prevPositions.begin();
        while (it != m_prevPositions.end())
        {
            if (!world.valid(it->first) ||
                !world.all_of<RenderStrategyTag<RenderStrategy::Interpolation>>(
                    it->first))
            {
                it = m_prevPositions.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Snapshot current positions.
        for (auto [e, body] :
             world
                 .view<RenderStrategyTag<RenderStrategy::Interpolation>,
                       const ComponentPhysicBody>()
                 .each())
        {
            m_prevPositions[e] = body.pos;
        }
    }

    // Write ComponentInterpolatedTransform for Interpolation-tagged entities.
    // Call AFTER Scene::Update, passing the scheduler's interpolation alpha.
    void PostUpdate(GameWorld& world, float alpha)
    {
        alpha = math::clamp(alpha, 0.f, 1.f);

        for (auto [e, body] :
             world
                 .view<RenderStrategyTag<RenderStrategy::Interpolation>,
                       const ComponentPhysicBody>()
                 .each())
        {
            math::vec3 prev = body.pos;  // fallback for first frame
            auto it = m_prevPositions.find(e);
            if (it != m_prevPositions.end())
            {
                prev = it->second;
            }

            math::vec3 interpPos = math::lerp(prev, body.pos, alpha);
            world.emplace_or_replace<ComponentInterpolatedTransform>(
                e, interpPos);
        }
    }

   private:
    std::unordered_map<entt::entity, math::vec3> m_prevPositions;
};

}  // namespace game
