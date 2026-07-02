#include "Engine/Graphics/Renderer.hpp"

#include "Engine/AssetBundle.hpp"
#include "Engine/GameAppState.hpp"
#include "stb_easy_font.h"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"
#include "RendererInternals.hpp"

namespace OneGame::Engine::Graphics
{

void Renderer::Initialize(AssetContext& assets)
{
    uniformArena.Initialize(assets.backend, assets.backend.MaxUniformBufferSize() > 1024 * 1024 * 16
                                                ? 1024 * 1024 * 16
                                                : assets.backend.MaxUniformBufferSize());
    LOG_DEBUG("uniform arena created");

    InitContext ctx{assets, uniformArena};
    debugInfoPass.Enable(assets.backend, ctx);
    LOG_DEBUG("debug info pass created");
    // testPass.Enable(backend, ctx);
    // LOG_DEBUG("test pass created");
    // terrainPass.Enable(backend, ctx);
    // LOG_DEBUG("terrain pass created");
    terrainPass2.Enable(assets.backend, ctx);
    LOG_DEBUG("terrain pass 2 created");
    // uiPass.Initialize(backend, appCtxt);
}

void Renderer::Shutdown(AssetContext& assets)
{
    // uiPass.Shutdown(backend);
    terrainPass2.Disable(assets.backend);
    // terrainPass.Disable(backend);
    // testPass.Disable(backend);
    debugInfoPass.Disable(assets.backend);
    uniformArena.Shutdown(assets.backend);
}

void Renderer::Draw(DrawContext& drawCtxt)
{
    // testPass.Draw(drawCtxt);
    // if (enableTerrainPass)
    //     terrainPass.Draw(drawCtxt);
    terrainPass2.Draw(drawCtxt);
    debugInfoPass.Draw(drawCtxt);
}

// void Renderer::EnableTerrainPass(AssetContext& assets, const std::vector<std::string>& textureIdArray)
// {
//     auto copyIdArray = textureIdArray;
//     while (copyIdArray.size() < 256)
//     {
//         copyIdArray.emplace_back("invalid.png");
//     }
//     auto tex = assets.LoadTextureArray(BLOCK_TEXTURE_SIZE, BLOCK_TEXTURE_SIZE, copyIdArray);
//     InitContext ctx = {assets, uniformArena};
//     terrainPass2.Enable(assets.backend, ctx, tex);
// }

void Renderer::RenderView(AssetContext& assets, DrawContext ctxt)
{
    auto pview = ctxt.currentView == entt::null ? nullptr : ctxt.world.try_get<PViewTransform>(ctxt.currentView);
    math::mat4 view =
        pview ? pview->view : math::lookAt(math::vec3(20, 20, 20), math::vec3(0, 0, 0), math::vec3(0, 1, 0));
    auto pproj = ctxt.currentView == entt::null ? nullptr : ctxt.world.try_get<PPerspectiveTransform>(ctxt.currentView);
    math::mat4 proj = math::get_perspective_rot(ctxt.backend.SwapchainPretransform()) *
                      (pproj ? math::perspective_rev_z(pproj->fov, pproj->aspect, 0.1f)
                             : math::perspective_rev_z(math::radians(45.0f), ctxt.backend.SwapchainAspect(), 0.1f));
    ctxt.pvTransform = proj * view;

    if (ctxt.currentView == entt::null)
    {
        auto extent = assets.backend.SwapchainExtent();
        ctxt.drawCmd.SetViewRect(0, 0, extent.x, extent.y);
    }
    else
    {
        auto& rect = ctxt.world.get<PGameView>(ctxt.currentView);
        ctxt.drawCmd.SetViewRect(rect.pos.x, rect.pos.y, rect.extent.x, rect.extent.y);
    }

    Draw(ctxt);
}

void Renderer::Render(AssetContext& assets, const entt::registry& world, float deltaTime)
{
    auto& backend = assets.backend;
    auto& cmd = backend.CreateCommandList(QueueType::Present);
    auto& tCmd = backend.CreateCommandList(QueueType::Transfer);

    math::mat4 mat = math::identity();
    DrawContext drawCtxt = {backend, uniformArena, chunkAllocator, deltaTime, cmd, tCmd, world, mat};

    cmd.Begin();
    tCmd.Begin();

    ClearValues values{};
    values.colorClears[0] = {0.1f, 0.2f, 0.4f, 1.0f};
    values.depthClear = 0.0f;
    values.stencilClear = 0.f;

    drawCtxt.drawCmd.BeginRenderPass(backend.GetCurrentRenderPass(), backend.GetCurrentFrameBuffer(), values);

    for (auto entity : world.view<PGameView>())
    {
        drawCtxt.currentView = entity;
        RenderView(assets, drawCtxt);
    }

    // draw overlay
    drawCtxt.currentView = entt::null;
    RenderView(assets, drawCtxt);

    drawCtxt.drawCmd.EndRenderPass();

    tCmd.End();
    cmd.End();

    uniformArena.Flush(backend);
    uniformArena.AdvanceFrame();
}

// void Renderer::EnableTerrainPass(AssetContext& assets, Mesh terrainMesh)
// {
//     InitContext initCtx{assets, uniformArena};
//     // terrainPass.Enable(backend, initCtx, terrainMesh);
//     enableTerrainPass = true;
// }

// void Renderer::DisableTerrainPass(AssetContext& assets)
// {
//     // terrainPass.Disable(backend);
//     enableTerrainPass = false;
// }

// void Renderer::EnableTerrainPass2(AssetContext& assets, GPUBufferHandle storageBuf)
// {
//     InitContext initCtx{assets, uniformArena};
//     terrainPass2.Enable(assets.backend, initCtx);
//     enableTerrainPass2 = true;
// }

// void Renderer::DisableTerrainPass2(AssetContext& assets)
// {
//     terrainPass2.Disable(assets.backend);
//     enableTerrainPass2 = false;
// }

}  // namespace OneGame::Engine::Graphics
