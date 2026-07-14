#pragma once

#include <vector>

#include "Engine/ECS/GameWorld.hpp"
#include "Engine/ECS/IRenderer.hpp"
#include "Engine/ECS/RendererRegistry.hpp"
#include "Engine/GraphicState.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Terrain/TerrainRenderer.hpp"
#include "Engine/entt.hpp"

namespace OneGame::Engine::ECS
{
class GameRenderer
{
   public:
    struct Def
    {
        std::vector<std::string> renderers;
    };

    static GameRenderer Build(Def def, PresentationContext& ctx)
    {
        return GameRenderer(std::move(DefBuilder::BuildABCVec<RendererBase>(def.renderers, ctx.rendererRegistry)));
    }

    class Builder
    {
       public:
        template <typename TRenderer>
        Builder&& With() &&
        {
            m_def.renderers.emplace_back(TRenderer::Name);
            return std::move(*this);
        }

        GameRenderer Build(PresentationContext& ctx) && { return GameRenderer::Build(m_def, ctx); }

       private:
        Def m_def;
    };

    GameRenderer(std::vector<std::unique_ptr<RendererBase>> renderers = {}) : m_renderers(std::move(renderers)) {}

    void Initialize(GameWorldContext& world, PresentationContext ctx)
    {
        auto rootView = world.create();
        world.emplace<ScreenRect>(rootView, I16Point2{0, 0}, ctx.backend.SwapchainExtent());
        world.emplace<UIRoot>(rootView);

        if (auto blks = world.ctx().find<Terrain::BlockRegistry>())
        {
            auto blkArray = blks->GetBlockTextureArray();
            for (uint32_t i = 0; i < blkArray.size(); ++i)
            {
                ctx.renderer.UpdateBlockTexture(ctx, blkArray[i], i);
            }
        }

        for (auto& ptr : m_renderers)
        {
            ptr->Initialize(world, ctx);
        }
    }

    void Present(GameWorldContext& game, PresentationContext pctx, FrameOutputData& frameOut)
    {
        for (auto& ptr : m_renderers)
        {
            ptr->Present(game, pctx, frameOut);
        }
    }

   private:
    std::vector<std::unique_ptr<RendererBase>> m_renderers;
};
}  // namespace OneGame::Engine::ECS
