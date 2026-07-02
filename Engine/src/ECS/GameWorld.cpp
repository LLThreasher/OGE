#include "Engine/ECS/GameWorld.hpp"
#include "Engine/Graphics/Renderer.hpp"

namespace OneGame::Engine::ECS
{
    void GameWorld::InitializeWithPresent(PresentationContext ctx)
    {
        Initialize(ctx);
        auto blkArray = m_blocks.GetBlockTextureArray();
        for (uint32_t i = 0; i < blkArray.size(); ++i)
        {
            ctx.renderer.UpdateBlockTexture(ctx, blkArray[i], i);
        }
    }
}
