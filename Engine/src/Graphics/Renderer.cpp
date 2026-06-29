#include "Engine/Graphics/Renderer.hpp"

#include "Engine/GameAppState.hpp"
#include "Engine/AssetBundle.hpp"
#include "stb_easy_font.h"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"
#include "RendererInternals.hpp"

namespace OneGame::Engine::Graphics
{

void Renderer::Initialize(AssetContext& assets)
{
    uniformArena.Initialize(
        assets.backend, assets.backend.MaxUniformBufferSize() > 1024 * 1024 * 16 ? 1024 * 1024 * 16 : assets.backend.MaxUniformBufferSize());
    LOG_DEBUG("uniform arena created");

    InitContext ctx{assets, uniformArena};
    debugInfoPass.Enable(assets.backend, ctx);
    LOG_DEBUG("debug info pass created");
    // testPass.Enable(backend, ctx);
    // LOG_DEBUG("test pass created");
    // terrainPass.Enable(backend, ctx);
    // LOG_DEBUG("terrain pass created");
    // terrainPass2.Enable(backend, ctx);
    // LOG_DEBUG("terrain pass 2 created");
    // uiPass.Initialize(backend, appCtxt);
}

void Renderer::Shutdown(AssetContext& assets)
{
    // uiPass.Shutdown(backend);
    // terrainPass2.Disable(backend);
    // terrainPass.Disable(backend);
    // testPass.Disable(backend);
    debugInfoPass.Disable(assets.backend);

    uniformArena.Shutdown(assets.backend);
}

void Renderer::Prepare(AssetContext& assets, entt::registry& world, float deltaTime)
{
    PrepareContext pc{
        assets.backend,
        deltaTime,
        world,
    };
    // uiPass.Prepare(world);
    terrainPass2.Prepare(pc);
    // terrainPass.Prepare(pc);
    debugInfoPass.Prepare(pc);
    // testPass.Prepare(pc);
}

void Renderer::Draw(DrawContext& drawCtxt)
{
    // testPass.Draw(drawCtxt);
    // if (enableTerrainPass)
    //     terrainPass.Draw(drawCtxt);
    if (enableTerrainPass2) terrainPass2.Draw(drawCtxt);
    debugInfoPass.Draw(drawCtxt);
}

void Renderer::Render(IGraphicsBackend& backend, float deltaTime)
{
    auto& tCmd = backend.CreateCommandList(QueueType::Transfer);
    auto& cmd = backend.CreateCommandList(QueueType::Present);
    cmd.Begin();

    ClearValues values{};
    values.colorClears[0] = {0.1f, 0.2f, 0.4f, 1.0f};
    values.depthClear = 0.0f;
    values.stencilClear = 0.f;
    cmd.BeginRenderPass(backend.GetCurrentRenderPass(), backend.GetCurrentFrameBuffer(), values);

    DrawContext drawCtxt = {
        backend, deltaTime, uniformArena, cmd, tCmd,
    };

    tCmd.Begin();
    Draw(drawCtxt);
    tCmd.End();

    cmd.EndRenderPass();
    cmd.End();

    uniformArena.Flush(backend);
    uniformArena.AdvanceFrame();
}

void Renderer::EnableTerrainPass(AssetContext& assets, Mesh terrainMesh)
{
    InitContext initCtx{assets, uniformArena};
    // terrainPass.Enable(backend, initCtx, terrainMesh);
    enableTerrainPass = true;
}

void Renderer::DisableTerrainPass(AssetContext& assets)
{
    // terrainPass.Disable(backend);
    enableTerrainPass = false;
}

void Renderer::EnableTerrainPass2(AssetContext& assets, GPUBufferHandle storageBuf)
{
    InitContext initCtx{assets, uniformArena};
    terrainPass2.Enable(assets.backend, initCtx, storageBuf);
    enableTerrainPass2 = true;
}

void Renderer::DisableTerrainPass2(AssetContext& assets)
{
    terrainPass2.Disable(assets.backend);
    enableTerrainPass2 = false;
}

}  // namespace OneGame::Engine::Graphics
