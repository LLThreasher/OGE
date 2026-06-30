#include "Engine/Scenes/DebugScene.hpp"

#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Random.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/Terrain.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
void DebugScene2::Initialize(PresentationContext context)
{
    using namespace ECS;
    gameWorld.Register<SubsystemUI>();
    gameWorld.Register<SubsystemPlayerInput>();
    gameWorld.Register<SubsystemCamera>();
    gameWorld.Register<SubsystemDebugInfo>();

    auto e = UI::CreateGameView(gameWorld.Get().world, {0, math::vec2{0, 0}, math::vec2{1, 1}});
}

void DebugScene2::Enter(PresentationContext context)
{
    AppContext actx = context;
    gameWorld.Initialize(actx);

    using namespace Terrain;
    using BufferUsage = Graphics::BufferUsage;
    // for (size_t x = 0; x < 16; ++x)
    //{
    //	for (size_t y = 0; y < 16; ++y)
    //	{
    //		for (size_t z = 0; z < 16; ++z)
    //		{
    //			if (x == 1 || x == 2 || x == 3 || x == 4)
    //			{
    //				auto idx = Terrain::GetBlockIndex(x, y, z);
    //				chunk->data[idx] = 256;
    //			}
    //		}
    //	}
    // }

    // auto handle = terrainData.chunkData.Create();
    // auto chunk = terrainData.chunkData.Get(handle);
    // chunk->Coords = { cx, cy, cz };
    // terrainData.coordToChunks[{cx, cy, cz}] = handle;
    // for (size_t i = 0; i < 200; ++i)
    //{
    //	auto x = Random::RandInt(0, 15);
    //	auto y = Random::RandInt(0, 15);
    //	auto z = Random::RandInt(0, 15);
    //	assert(x < 16);
    //	assert(y < 16);
    //	assert(z < 16);

    //	auto idx = Terrain::GetBlockIndex(x, y, z);
    //	assert(idx < 16 * 16 * 16);
    //	chunk->data[idx] = 256;
    //}

    static int chunkCount = 32;
    // Graphics::ChunkAllocator gpuBufferAllocator;

    auto generate_terrain = [](int cx, int cy, int cz, int x, int y, int z)
    {
        // if (cx > -3 && cx < 2 && cy > 0 && cy < 3 && cz > -3 && cz < 2)
        //	return 0;
        if (((x + y + z) % 2) == 0)
        {
            return 1;
        }
        return 0;
    };

    LOG_DEBUG("start generate terrain");
    for (int cx = -10; cx < 10; ++cx)
    {
        for (int cz = -10; cz < 10; ++cz)
        {
            for (int cy = 0; cy < 5; ++cy)
            {
                auto handle = terrainData.chunks.AllocateChunk({cx, cy, cz});
                auto chunk = terrainData.chunks.Get(handle);

                for (size_t z = 0; z < 16; ++z)
                {
                    for (size_t y = 0; y < 16; ++y)
                    {
                        for (size_t x = 0; x < 16; ++x)
                        {
                            chunk->SetBlock(x, y, z, generate_terrain(cx, cy, cz, x, y, z));
                        }
                    }
                }
            }
        }
    }
    LOG_DEBUG("end generate terrain");

    LOG_DEBUG("start generate mesh");

    auto [handle, chunk] = terrainData.chunks.Get({0, 0, 0});
    assert(handle.IsValid());
    auto coord = chunk->Coords;
    LOG_DEBUG("queuing chunk ({}, {}, {}) with ({}, {}, {})", 0, 0, 0, coord.x, coord.y, coord.z);
    terrainPresData.buildMeshQueue.push(handle);

    // for (int cx = -3; cx < 3; ++cx)
    //{
    //	for (int cz = -3; cz < 3; ++cz)
    //	{
    //		for (int cy = 0; cy < 4; ++cy)
    //		{
    //			auto [handle, chunk] = terrainData.chunks.Get({ cx, cy,
    // cz }); 			assert(handle.IsValid()); 			auto coord = chunk->Coords;
    //			LOG_DEBUG("queuing chunk ({}, {}, {}) with ({}, {},
    //{})", cx, cy, cz, coord.x, coord.y, coord.z);
    //			terrainData.buildMeshQueue.push(handle);
    //		}
    //	}
    // }
    BlockRegistry blocks;
    blocks.RegisterBlock("dirt", {"Dirt", {0, 1, 2, 3, 4, 5}, 1});
    meshBuilder.SetVertexBudget(192 * 1024 * 1024);
    meshBuilder.BuildChunkMeshes(terrainData, blocks, terrainPresData);
    LOG_DEBUG("end generate mesh");

    // terrainPresData.terrainMesh =
    //     context.backend.AllocateGPUBuffer<BufferUsage::Storage>(chunkCount * 4 * Terrain::CHUNK_STORE_BYTE_SIZE);
    // auto bundle = context.assetPool;
    // context.renderer.EnableTerrainPass2(context, terrainPresData.terrainMesh);

    // int active_count = chunkCount - 23;
    // LOG_INFO("total used size: {} {}", active_count, active_count * Terrain::CHUNK_STORE_BYTE_SIZE * 4);
    // int currentSlot = 0;
    // auto event = context.streamingManager.CreateResourceBundle(
    //     [&]
    //     {
    //         isTerrainReady = true;
    //         auto& world = gameWorld.Get().world;
    //         auto pe = world.view<ECS::PlayerViewPanel>().front();
    //         world.emplace<ECS::UIFocus>(pe);
    //     });
    // while (!terrainPresData.uploadMeshQueue.empty() && currentSlot < active_count)
    // {
    //     auto [chunkHandle, builtMeshHandle] = std::move(terrainPresData.uploadMeshQueue.front());
    //     terrainPresData.uploadMeshQueue.pop();
    //     auto builtMesh = terrainPresData.builtChunkMeshes.Get(builtMeshHandle);
    //     auto chunkCoord = terrainData.chunks.Get(chunkHandle)->Coords;
    //     context.streamingManager.UploadBuffer<UploadType::Async, BufferUsage::Storage>(
    //         builtMesh->quads, terrainPresData.terrainMesh, currentSlot * Terrain::CHUNK_STORE_BYTE_SIZE, event);
    //     testSlots.push_back(Graphics::PTerrainMesh{currentSlot * Terrain::CHUNK_STORE_BYTE_SIZE,
    //                                                static_cast<uint32_t>(builtMesh->quads.size()) * 6, chunkCoord.x,
    //                                                chunkCoord.y, chunkCoord.z});
    //     currentSlot += 4;
    //     assert(builtMesh->quads.size() * sizeof(Terrain::TexturedQuad) <= Terrain::CHUNK_STORE_BYTE_SIZE * 4);
    //     LOG_DEBUG("looking at chunk ({}, {}, {}), data size: {} kb", chunkCoord.x, chunkCoord.y, chunkCoord.z,
    //               (float)(builtMesh->quads.size() * sizeof(Terrain::TexturedQuad)) / 1024.f);
    // }
    // LOG_DEBUG("used slots {}", currentSlot);
}

void DebugScene2::Exit(PresentationContext context) {}

void DebugScene2::Update(PresentationContext context, const FrameInputData& frame, FrameOutputData& frameOut)
{
    auto& world = frameOut.presentationWorld;
    if (isTerrainReady)
    {
        for (auto& slot : testSlots)
        {
            auto chunkEntity = world.create();
            world.emplace<Graphics::PTerrainMesh>(chunkEntity, slot);
        }
    }

    gameWorld.Update(context, frame);
    gameWorld.Present(context, frameOut);

    // if (frame.input.IsKeyDown(KeyCode::KY_G))
    // {
    //     auto& gworld = gameWorld.Get().world;
    //     auto pe = gworld.view<ECS::PlayerInputSource>().front();
    //     gworld.patch<ECS::PlayerInputSource>(pe, [](auto& in){
    //         in.source |= ECS::InputSource::KeyMouse;
    //     });
    // }
    // else if (frame.input.IsKeyDown(KeyCode::KY_ESCAPE))
    // {
    //     auto& gworld = gameWorld.Get().world;
    //     auto pe = gworld.view<ECS::PlayerInputSource>().front();
    //     gworld.patch<ECS::PlayerInputSource>(pe, [](auto& in){
    //         in.source &= ~ECS::InputSource::KeyMouse;
    //     });
    // }
}

void DebugScene3::Initialize(PresentationContext context)
{
    using namespace ECS;
    m_gameWorld.Register<SubsystemUI>();
    m_gameWorld.Register<SubsystemPlayerInput>();
    m_gameWorld.Register<SubsystemCamera>();
    m_gameWorld.Register<SubsystemDebugInfo>();
    m_gameWorld.Register<SubsystemPlayer>();

    auto& blocks = m_gameWorld.Get().blocks;
    blocks.RegisterBlock("dirt", {"Dirt", {2, 2, 2, 2, 2, 2}, 1});
    blocks.RegisterBlock("wood", {"Wood", {4, 4, 4, 4, 4, 4}, 1});
    blocks.RegisterBlock("stone", {"Stone", {5, 5, 5, 5, 5, 5}, 1});

    Terrain::TerrainDesc desc{};
    desc.chunkViewDistance = 4;
    m_gameWorld.Get().world.emplace<Terrain::TerrainDesc>(m_gameWorld.Get().world.create(), desc);
}

void DebugScene3::Enter(PresentationContext context)
{
    using namespace ECS;
    m_gameWorld.Initialize(context);
    auto vpe = UI::CreateGameView(m_gameWorld.Get().world, {0, math::vec2{0, 0}, math::vec2{1, 1}});

    auto& world = m_gameWorld.Get().world;
    auto player = ComponentPlayer::CreatePlayer(world);
    {
        ComponentCamera& cam = world.get<ComponentCamera>(player);
        cam.position = {20.f, 20.f, 20.f};

        glm::vec3 target = {0.f, 0.f, 0.f};
        cam.forward = glm::normalize(target - cam.position);

        cam.yaw = std::atan2(cam.forward.x, cam.forward.z);
        cam.pitch = std::asin(cam.forward.y);

        ComponentPerspectiveCamera& pcam = world.get<ComponentPerspectiveCamera>(player);
        pcam.fov = math::radians(45.f);
    }
    world.get<ViewPanel>(vpe).activeCamera = player;
    world.patch<ViewPanel>(vpe);
}

void DebugScene3::Exit(PresentationContext context)
{
}

void DebugScene3::Update(PresentationContext context, const FrameInputData& frame, FrameOutputData& frameOut)
{
    auto& blocks = m_gameWorld.Get().blocks;

    m_gameWorld.Update(context, frame);
    m_gameWorld.Present(context, frameOut);

    if (frame.input.IsKeyDown(KeyCode::KY_G))
    {
        auto& gworld = m_gameWorld.Get().world;
        auto pe = gworld.view<ECS::PlayerInputData>().front();
        assert(gworld.valid(pe));
        gworld.get_or_emplace<ECS::InputSourceKeyMouse>(pe);
    }
    else if (frame.input.IsKeyDown(KeyCode::KY_ESCAPE))
    {
        auto& gworld = m_gameWorld.Get().world;
        auto pe = gworld.view<ECS::PlayerInputData>().front();
        if (gworld.all_of<ECS::InputSourceKeyMouse>(pe))
            gworld.erase<ECS::InputSourceKeyMouse>(pe);
    }
}
}  // namespace OneGame::Engine
