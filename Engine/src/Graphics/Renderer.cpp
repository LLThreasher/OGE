#include "Engine/Graphics/Renderer.hpp"
#include "stb_easy_font.h"

namespace OneGame::Engine::Graphics
{

    void Renderer::Initialize(IGraphicsBackend* backend, AssetManager& assets)
    {
        uniformArena.Initialize(backend, backend->MaxUniformBufferSize());
        ringStagingBuffer.Initialize(backend, 1024 * 1024 * 16);
        InitContext ctx { assets, uniformArena, ringStagingBuffer };
        debugInfoPass.Initialize(backend, ctx);
        testPass.Initialize(backend, ctx);
        //uiPass.Initialize(backend, appCtxt);
    }

    void Renderer::Shutdown(IGraphicsBackend* backend)
    {
        //uiPass.Shutdown(backend);
        //testPass.Shutdown(backend);
        //debugInfoPass.Shutdown(backend);
        //ringStagingBuffer.Shutdown(backend);
        //uniformArena.Shutdown(backend);
    }

    void Renderer::Prepare(entt::registry* world)
    {
        //uiPass.Prepare(world);
        debugInfoPass.Prepare(world);
        testPass.Prepare(world);
    }

    void Renderer::Render(IGraphicsBackend* backend, float deltaTime)
    {
        auto transferCmd = backend->CreateCommandList(QueueType::Transfer);
        auto cmd = backend->CreateCommandList(QueueType::Present);

        transferCmd->Begin();
        cmd->Begin();

        ClearValues values{};
        values.colorClears[0] = { 0.1f, 0.2f, 0.4f, 1.0f };
        values.depthClear = 1.0f;
        values.stencilClear = 0.f;
        cmd->BeginRenderPass(backend->GetCurrentRenderPass(), backend->GetCurrentFrameBuffer(), values);

        DrawContext drawCtxt = {
            deltaTime,
            &uniformArena,
            cmd.get(),
            backend,
            transferCmd.get(),
        };

        debugInfoPass.Draw(drawCtxt);
        testPass.Draw(drawCtxt);

        cmd->EndRenderPass();
        cmd->End();
        transferCmd->End();

        uniformArena.AdvanceFrame();
    }
}
