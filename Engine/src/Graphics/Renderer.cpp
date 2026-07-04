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
    uniformArena.Initialize(assets.backend, 1024 * 1024 * 32);
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
    auto& views = ctxt.world.Get<CmdAddView>();
    if (views.empty()) return;
    auto cmdview = views[0];
    math::mat4 proj = math::get_perspective_rot(ctxt.backend.SwapchainPretransform()) *
                      (math::perspective_rev_z(cmdview.fov, cmdview.aspect == 0.f ? assets.backend.SwapchainAspect() : cmdview.aspect, 0.1f));
    ctxt.pvTransform = proj * cmdview.view;

    auto& rect = cmdview.rect;
    ctxt.drawCmd.SetViewRect(rect.pos.x, rect.pos.y, rect.extent.x, rect.extent.y);

    Draw(ctxt);
}

void Renderer::Render(AssetContext& assets, SubmissionQueue& world, float deltaTime)
{
    auto& backend = assets.backend;
    auto& cmd = backend.CreateCommandList(QueueType::Present);
    auto& tCmd = backend.CreateCommandList(QueueType::Transfer);

    world.Add<CmdAddView>(GameViewType::Overlay, IRect{{0, 0}, assets.backend.SwapchainExtent()});

    cmd.Begin();
    tCmd.Begin();

    ClearValues values{};
    values.colorClears[0] = {0.1f, 0.2f, 0.4f, 1.0f};
    values.depthClear = 0.0f;
    values.stencilClear = 0.f;

    cmd.BeginRenderPass(backend.GetCurrentRenderPass(), backend.GetCurrentFrameBuffer(), values);

    for (auto view : ALL_GAME_VIEWS)
    {
        DrawContext drawCtxt = {backend, uniformArena, chunkAllocator, deltaTime, cmd, tCmd, world.GetSingle(view), view};
        RenderView(assets, drawCtxt);
    }

    // draw overlay
    {
        DrawContext drawCtxt = {backend, uniformArena, chunkAllocator, deltaTime, cmd, tCmd, world.GetSingle(GameViewType::Overlay)};
        RenderView(assets, drawCtxt);
    }

    cmd.EndRenderPass();

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
