#pragma once

#include <vector>

#include "Engine/ECS/GameWorld.hpp"
#include "Engine/ECS/IRenderer.hpp"
#include "Engine/GraphicState.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Terrain/TerrainRenderer.hpp"
#include "Engine/entt.hpp"

namespace OneGame::Engine::ECS
{
class GameRenderer
{
   public:
    GameRenderer(GameWorld& world) : m_world(world.m_world) {}

    template <typename TSubsystem>
    void Register()
    {
        m_subsystems.emplace_back(new TSubsystem());
    }

    void Initialize(PresentationContext ctx)
    {
        auto rootView = m_world.create();
        m_world.emplace<ScreenRect>(rootView, I16Point2{0, 0}, ctx.backend.SwapchainExtent());
        m_world.emplace<UIRoot>(rootView);

        if (auto blks = Get().ctx().find<Terrain::BlockRegistry>())
        {
            auto blkArray = blks->GetBlockTextureArray();
            for (uint32_t i = 0; i < blkArray.size(); ++i)
            {
                ctx.renderer.UpdateBlockTexture(ctx, blkArray[i], i);
            }
        }

        for (auto& ptr : m_subsystems)
        {
            ptr->Initialize(m_world, ctx);
        }
    }

    void Present(PresentationContext pctx, FrameOutputData& frameOut)
    {
        GameWorldContext& game = Get();
        for (auto& ptr : m_subsystems)
        {
            ptr->Present(game, pctx, frameOut);
        }
    }

    GameWorldContext& Get() { return m_world; }

   private:
    std::vector<std::unique_ptr<RendererBase>> m_subsystems;
    entt::registry& m_world;
};
}  // namespace OneGame::Engine::ECS
