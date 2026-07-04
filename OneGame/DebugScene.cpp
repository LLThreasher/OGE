#include "DebugScene.hpp"

#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Random.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/TerrainService.hpp"
#include "Engine/Terrain/TerrainRenderer.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"
#include "Engine/ECS/IRenderer.hpp"
#include "Engine/ECS/ISubsystem.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame::Engine
{
void DebugScene3::Initialize(PresentationContext& context)
{
    using namespace ECS;
    m_gameWorld.Register<Terrain::TerrainService>();
    m_gameWorld.Register<SubsystemUI>();
    m_gameWorld.Register<SubsystemPlayerInput>();
    m_gameWorld.Register<SubsystemPlayer>();

    m_gameRenderer.Register<Terrain::TerrainRenderer>();
    m_gameRenderer.Register<DebugInfoRenderer>();
    m_gameRenderer.Register<UIRenderer>();
    m_gameRenderer.Register<CameraRenderer>();
    m_gameRenderer.Register<PlayerInputRenderer>();

    auto& blocks = m_gameWorld.Get().ctx().get<Terrain::BlockRegistry>();
    blocks.RegisterBlock("dirt", {"Dirt", "dirt.png", 1});
    blocks.RegisterBlock("wood", {"Wood", "wood_plank.png", 1});
    blocks.RegisterBlock("stone", {"Stone", "green_stone.png", 1});

    Terrain::TerrainDesc desc{};
    desc.chunkViewDistance = 16;
    m_gameWorld.Get().ctx().emplace<Terrain::TerrainDesc>(desc);
}

void DebugScene3::Enter(PresentationContext& context)
{
    using namespace ECS;
    m_gameWorld.Initialize(context);
    m_gameRenderer.Initialize(context);
    auto vpe = UI::CreateGameView(m_gameWorld.Get(), {math::vec2{0, 0}, math::vec2{1, 1}});

    auto& world = m_gameWorld.Get();
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

    // put something in the middle of the screen
    auto cubeEntity = world.create();
    world.emplace<UIRect>(cubeEntity, math::vec2{0.5f - 0.01f, 0.5f - 0.01f * context.backend.SwapchainAspect()}, math::vec2{0.01f, 0.01f * context.backend.SwapchainAspect()});
    world.emplace<UIZLevel>(cubeEntity, 1);
    world.emplace<UIRaycastTarget>(cubeEntity);

    // create move widget
    auto scaledX = 0.3f;
    auto scaledY = scaledX * context.backend.SwapchainAspect();

    auto lookWidget = world.create();
    world.emplace<UIRect>(lookWidget, math::vec2{0.0f, 0.0f}, math::vec2{1.0f, 1.0f});
    world.emplace<UIZLevel>(lookWidget, 0);
    world.emplace<UIRaycastTarget>(lookWidget);
    
    auto mvWidget = world.create();
    world.emplace<UIRect>(mvWidget, math::vec2{0.0f, 1.f - scaledY},
                          math::vec2{scaledX, scaledY});
    world.emplace<UIZLevel>(mvWidget, 1);
    world.emplace<UIRaycastTarget>(mvWidget);

    world.emplace<InputSourceWidget>(player, mvWidget, lookWidget);
}

void DebugScene3::Exit(PresentationContext& context)
{
}

void DebugScene3::Update(PresentationContext& context, const FrameInputData& frame, FrameOutputData& frameOut)
{
    auto& blocks = m_gameWorld.Get();

    m_gameWorld.Update(context, frame);
    m_gameRenderer.Present(context, frameOut);

    if (frame.input.IsKeyDown(KeyCode::KY_G))
    {
        auto& gworld = m_gameWorld.Get();
        auto pe = gworld.view<ECS::PlayerInputData>().front();
        assert(gworld.valid(pe));
        gworld.get_or_emplace<ECS::InputSourceKeyMouse>(pe);
    }
    else if (frame.input.IsKeyDown(KeyCode::KY_ESCAPE))
    {
        auto& gworld = m_gameWorld.Get();
        auto pe = gworld.view<ECS::PlayerInputData>().front();
        if (gworld.all_of<ECS::InputSourceKeyMouse>(pe))
            gworld.erase<ECS::InputSourceKeyMouse>(pe);
    }
}
}  // namespace OneGame::Engine
