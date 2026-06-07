#pragma once

#include "UniformArena.hpp"
#include "IGraphicsBackend.hpp"
#include "../Math.hpp"

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

    class TestRendererCubeWithMVP
    {
        struct UBO
        {
            math::mat4 model;
            math::mat4 view;
            math::mat4 proj;
        };

        struct Vertex
        {
            math::vec3 pos;
            math::vec3 color;
        };

    public:
        void Initialize(IGraphicsBackend* backend);
        void Render(IGraphicsBackend* backend, float deltaTime);

    private:
        float m_Time = 0.0f;
        float angle = 0.0f;
        GPUPipelineHandle pipeline;
        GPUBindingGroupLayoutHandle bindingGroupLayout;
        GPUBufferHandle vertexBuffer;
        GPUBufferHandle indexBuffer;
        //std::vector<GPUBindingGroupHandle> bindingGroupPerFrame;
        //std::vector<GPUBufferHandle> uniformBufferPerFrame;
        UniformArena uniformArena;
        GPUBindingGroupHandle bindingGroup;

        bool isFirstFrame = true;

        const std::vector<Vertex> vertices = {
            {{-1,-1,-1},{1,0,0}},
            {{ 1,-1,-1},{0,1,0}},
            {{ 1, 1,-1},{0,0,1}},
            {{-1, 1,-1},{1,1,0}},
            {{-1,-1, 1},{1,0,1}},
            {{ 1,-1, 1},{0,1,1}},
            {{ 1, 1, 1},{1,1,1}},
            {{-1, 1, 1},{0,0,0}},
        };

        const std::vector<uint32_t> indices = {
            0,1,2, 2,3,0, // back
            4,5,6, 6,7,4, // front
            0,4,7, 7,3,0, // left
            1,5,6, 6,2,1, // right
            3,2,6, 6,7,3, // top
            0,1,5, 5,4,0  // bottom
        };
    };
}
