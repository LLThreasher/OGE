#pragma once

#include "Engine/AssetBundle.hpp"
#include "Engine/Graphics/ChunkAllocator2.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Graphics/RingStagingBuffer.hpp"
#include "Engine/Graphics/SubmissionQueue.hpp"
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
    IGraphicsBackend& backend;
    UniformArena& uniformArena;
    DynamicChunkAllocator& chunkAllocator;
    float deltaTime;
    ICommandList& drawCmd;
    ICommandList& transferCmd;
    SingleSubmissionQueue& world;
    GameViewType currentView;
    math::mat4 pvTransform;
};

struct PrepareContext
{
};

}  // namespace OneGame::Engine::Graphics
