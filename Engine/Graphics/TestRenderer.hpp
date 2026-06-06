#pragma once

#include "IGraphicsBackend.hpp"

namespace OneGame::Engine::Graphics
{
    class TestRenderer
    {
    public:
        void Initialize(IGraphicsBackend* backend);
        void Render(IGraphicsBackend* backend, float deltaTime);

    private:
        float m_Time = 0.0f;
    };

    class TestRendererRotateTriangle
    {
    public:
        void Initialize(IGraphicsBackend* backend);
        void Render(IGraphicsBackend* backend, float deltaTime);

    private:
        float m_Time = 0.0f;
        float angle = 0.0f;
        GPUPipelineHandle pipeline;
    };
}
