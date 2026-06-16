#include "Engine/Graphics/Renderer.hpp"
#include "stb_easy_font.h"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine::Graphics
{

    void Renderer::Initialize(IGraphicsBackend* backend, AssetManager& assets)
    {
        uniformArena.Initialize(backend, backend->MaxUniformBufferSize() > 1024*1024*16 ? 1024*1024*16 : backend->MaxUniformBufferSize());
        LOG_DEBUG("uniform arena created");
        ringStagingBuffer.Initialize(backend, 1024 * 1024 * 16);
        LOG_DEBUG("staging buffer created");
        InitContext ctx { assets, uniformArena, ringStagingBuffer };
        debugInfoPass.Initialize(backend, ctx);
        LOG_DEBUG("debug info pass created");
        testPass.Initialize(backend, ctx);
        LOG_DEBUG("test pass created");
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

        testPass.Draw(drawCtxt);
        debugInfoPass.Draw(drawCtxt);

        cmd->EndRenderPass();
        cmd->End();
        transferCmd->End();

        uniformArena.AdvanceFrame();
    }
}
