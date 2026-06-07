#pragma once

#include "Engine/Graphics/UniformArena.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Math.hpp"

namespace OneGame
{
    using namespace Engine;
    using namespace Engine::Graphics;

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

    class TestRendererCubeTextured
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
            math::vec2 uv;
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
        UniformArena uniformArena;
        RingStagingBuffer ringStagingBuffer;
        GPUBindingGroupHandle bindingGroup;
        StagingAllocation textureBuffer;
        GPUTextureHandle texture;

        bool isFirstFrame = true;

        std::vector<Vertex> vertices =
        {
            // FRONT (+Z)
            {{-1,-1, 1}, {1,1,1}, {0,0}},
            {{ 1,-1, 1}, {1,1,1}, {1,0}},
            {{ 1, 1, 1}, {1,1,1}, {1,1}},
            {{-1, 1, 1}, {1,1,1}, {0,1}},

            // BACK (-Z)
            {{ 1,-1,-1}, {1,1,1}, {0,0}},
            {{-1,-1,-1}, {1,1,1}, {1,0}},
            {{-1, 1,-1}, {1,1,1}, {1,1}},
            {{ 1, 1,-1}, {1,1,1}, {0,1}},

            // LEFT (-X)
            {{-1,-1,-1}, {1,1,1}, {0,0}},
            {{-1,-1, 1}, {1,1,1}, {1,0}},
            {{-1, 1, 1}, {1,1,1}, {1,1}},
            {{-1, 1,-1}, {1,1,1}, {0,1}},

            // RIGHT (+X)
            {{ 1,-1, 1}, {1,1,1}, {0,0}},
            {{ 1,-1,-1}, {1,1,1}, {1,0}},
            {{ 1, 1,-1}, {1,1,1}, {1,1}},
            {{ 1, 1, 1}, {1,1,1}, {0,1}},

            // TOP (+Y)
            {{-1, 1, 1}, {1,1,1}, {0,0}},
            {{ 1, 1, 1}, {1,1,1}, {1,0}},
            {{ 1, 1,-1}, {1,1,1}, {1,1}},
            {{-1, 1,-1}, {1,1,1}, {0,1}},

            // BOTTOM (-Y)
            {{-1,-1,-1}, {1,1,1}, {0,0}},
            {{ 1,-1,-1}, {1,1,1}, {1,0}},
            {{ 1,-1, 1}, {1,1,1}, {1,1}},
            {{-1,-1, 1}, {1,1,1}, {0,1}},
        };

        std::vector<uint32_t> indices =
        {
            0,1,2, 2,3,0,        // front
            4,5,6, 6,7,4,        // back
            8,9,10, 10,11,8,     // left
            12,13,14, 14,15,12,  // right
            16,17,18, 18,19,16,  // top
            20,21,22, 22,23,20   // bottom
        };
    };
}
