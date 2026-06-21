#include "Engine/Graphics/Renderer.hpp"
#include "Engine/AssetBundle.hpp"
#include "stb_easy_font.h"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

#include "RendererInternals.hpp"

namespace OneGame::Engine::Graphics
{

    void Renderer::Initialize(IGraphicsBackend& backend, AssetManager& assets, StreamingManager& streaming)
    {
        uniformArena.Initialize(backend, backend.MaxUniformBufferSize() > 1024*1024*16 ? 1024*1024*16 : backend.MaxUniformBufferSize());
        LOG_DEBUG("uniform arena created");

        auto bundle = AssetBundleWriter(assets, streaming, backend);
        InitContext ctx { bundle, uniformArena };
        debugInfoPass.Enable(backend, ctx);
        LOG_DEBUG("debug info pass created");
        testPass.Enable(backend, ctx);
        LOG_DEBUG("test pass created");
        //terrainPass.Enable(backend, ctx);
        //LOG_DEBUG("terrain pass created");
        //terrainPass2.Enable(backend, ctx);
        //LOG_DEBUG("terrain pass 2 created");
        //uiPass.Initialize(backend, appCtxt);
    }

    void Renderer::Shutdown(IGraphicsBackend& backend)
    {
        //uiPass.Shutdown(backend);
        //terrainPass2.Disable(backend);
        //terrainPass.Disable(backend);
        testPass.Disable(backend);
        debugInfoPass.Disable(backend);

        uniformArena.Shutdown(backend);
    }

    void Renderer::Prepare(IGraphicsBackend& backend, entt::registry& world, float deltaTime)
    {
        PrepareContext pc
        {
            backend,
            deltaTime,
            world,
        };
        //uiPass.Prepare(world);
        terrainPass2.Prepare(pc);
        terrainPass.Prepare(pc);
        debugInfoPass.Prepare(pc);
        testPass.Prepare(pc);
    }

    

    void Renderer::Draw(DrawContext& drawCtxt)
    {
        //testPass.Draw(drawCtxt);
        if (enableTerrainPass)
            terrainPass.Draw(drawCtxt);
        else if (enableTerrainPass2)
            terrainPass2.Draw(drawCtxt);
        debugInfoPass.Draw(drawCtxt);
    }

    void Renderer::Render(IGraphicsBackend& backend, float deltaTime)
    {
        auto& tCmd = backend.CreateCommandList(QueueType::Transfer);
        auto& cmd = backend.CreateCommandList(QueueType::Present);
        cmd.Begin();

        ClearValues values{};
        values.colorClears[0] = { 0.1f, 0.2f, 0.4f, 1.0f };
        values.depthClear = 1.0f;
        values.stencilClear = 0.f;
        cmd.BeginRenderPass(backend.GetCurrentRenderPass(), backend.GetCurrentFrameBuffer(), values);

        DrawContext drawCtxt = {
            backend,
            deltaTime,
            uniformArena,
            cmd,
            tCmd,
        };

        tCmd.Begin();
        Draw(drawCtxt);
        tCmd.End();

        cmd.EndRenderPass();
        cmd.End();

        uniformArena.AdvanceFrame();
    }
    
    void Renderer::EnableTerrainPass(IGraphicsBackend& backend, AssetBundleWriter& bundle, Mesh terrainMesh)
    {
        InitContext initCtx { bundle, uniformArena };
        terrainPass.Enable(backend, initCtx, terrainMesh);
        enableTerrainPass = true;
    }

    void Renderer::DisableTerrainPass(IGraphicsBackend& backend)
    {
        terrainPass.Disable(backend);
        enableTerrainPass = false;
    }

    void Renderer::EnableTerrainPass2(IGraphicsBackend& backend, AssetBundleWriter& bundle, GPUBufferHandle storageBuf)
    {
        InitContext initCtx { bundle, uniformArena };
        terrainPass2.Enable(backend, initCtx, storageBuf);
        enableTerrainPass2 = true;
    }

    void Renderer::DisableTerrainPass2(IGraphicsBackend& backend)
    {
        terrainPass2.Disable(backend);
        enableTerrainPass2 = false;
    }

}
