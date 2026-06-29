#pragma once

#include "Engine/AssetBundle.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/Graphics/UniformArena.hpp"

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
    const IGraphicsBackend& backend;
    float deltaTime;
    UniformArena& uniformArena;
    ICommandList& drawCmd;
    ICommandList& transferCmd;
};

struct PrepareContext
{
    const IGraphicsBackend& backend;
    float deltaTime;
    entt::registry& world;
};
}  // namespace OneGame::Engine::Graphics
