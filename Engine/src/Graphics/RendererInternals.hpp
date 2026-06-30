#pragma once

#include "Engine/AssetBundle.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/Graphics/UniformArena.hpp"
#include "Engine/Graphics/ChunkAllocator2.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"

namespace OneGame::Engine
{
    struct AssetContext;
}

namespace OneGame::Engine::Graphics
{
struct InitContext
{
    AssetContext& assets;
    UniformArena& uniformArena;
};

struct DrawContext
{
    IGraphicsBackend& backend;
    UniformArena& uniformArena;
    DynamicChunkAllocator& chunkAllocator;
    float deltaTime;
    ICommandList& drawCmd;
    ICommandList& transferCmd;
    const entt::registry& world;
    math::mat4& pvTransform;
    entt::entity currentView;
};

struct PrepareContext
{
};

}  // namespace OneGame::Engine::Graphics
