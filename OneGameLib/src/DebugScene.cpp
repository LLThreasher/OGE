#include "OneGame/DebugScene.hpp"

#include "Engine/ECS/IRenderer.hpp"
#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"
#include "Engine/Graphics/Renderer.hpp"
#include "Engine/Random.hpp"
#include "Engine/StreamingManager.hpp"
#include "Engine/Terrain/BlockManager.hpp"
#include "Engine/Terrain/TerrainRenderer.hpp"
#include "Engine/Terrain/TerrainService.hpp"
#include "Engine/Terrain/TerrainVertexFormat.hpp"

#define LOGGER_NAME "Engine"
#include "Engine/Logger.hpp"

namespace OneGame
{
using namespace ECS;

void DebugScene3::Initialize(PresentationContext& context)
{
    m_gameWorld.CreateTerrain();
    m_gameUpdater = GameUpdateScheduler::Builder()
                        .With<Terrain::TerrainService>()
                        .With<SubsystemUI>()
                        .With<SubsystemPlayerInput>()
                        .With<SubsystemCreature>()
                        .With<SubsystemPlayer>()
                        .WithTickGroup("Physics", 1 / 60.f)
                        .With<SubsystemPhysics>("Physics")
                        .Build(context);

    m_gameRenderer = GameRenderer::Builder()
                         .With<DebugInfoRenderer>()
                         .With<Terrain::TerrainRenderer>()
                         .With<UIRenderer>()
                         .With<CameraRenderer>()
                         .With<PlayerInputRenderer>()
                         .Build(context);

    auto& blocks = m_gameWorld.Get().ctx().get<Terrain::BlockRegistry>();
    blocks.RegisterBlock("dirt", {"Dirt", "dirt.png", 1});
    blocks.RegisterBlock("wood", {"Wood", "wood_plank.png", 1});
    blocks.RegisterBlock("stone", {"Stone", "green_stone.png", 1});

    Terrain::TerrainDesc desc{};
    desc.chunkViewDistance = 1;
    m_gameWorld.Get().ctx().emplace<Terrain::TerrainDesc>(desc);
}

void DebugScene3::Enter(PresentationContext& context)
{
    using namespace ECS;
    m_gameUpdater.Initialize(m_gameWorld.Get(), context);
    m_gameRenderer.Initialize(m_gameWorld.Get(), context);
    auto vpe = UI::CreateGameView(m_gameWorld.Get(), {math::vec2{0, 0}, math::vec2{1, 1}});

    auto& world = m_gameWorld.Get();
    auto player = ComponentPlayer::CreatePlayer(world, {20.f, 20.f, 20.f});
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

    m_terminalButton = UI::CreateButton(world, context, {math::vec2{0.f, 0.f}, math::vec2{0.1f, 0.1f}});

    auto font = context.LoadASCIIBitmapFont16x6("om_large_plain_idx.png");

    // {
    //     auto e = world.create();
    //     world.emplace<UIRect>(e, math::vec2{0.3f - 0.01f, 0.3f - 0.01f * context.backend.SwapchainAspect()},
    //     math::vec2{0.1f, 0.1f * context.backend.SwapchainAspect()}); world.emplace<UISprite>(e, crossSprite);
    //     world.emplace<UIZLevel>(e, 1);
    // }

    // {
    //     auto e = world.create();
    //     world.emplace<UIRect>(e, math::vec2{0.3f - 0.01f, 0.7f - 0.01f * context.backend.SwapchainAspect()},
    //     math::vec2{0.1f, 0.1f * context.backend.SwapchainAspect()}); world.emplace<UISprite>(e, crossSprite);
    //     // world.emplace<UIText>(e, UIText{.font=font, .text="Hello world\nTerminal"});
    //     // world.emplace<UIZLevel>(e, 1);
    // }

    // put something in the middle of the screen
    UISprite crossSprite{.sprite = context.LoadTexture("cross.png")};
    auto cubeEntity = world.create();
    world.emplace<UIRect>(cubeEntity, math::vec2{0.5f - 0.01f, 0.5f - 0.01f * context.backend.SwapchainAspect()},
                          math::vec2{0.01f, 0.01f * context.backend.SwapchainAspect()});
    world.emplace<UISprite>(cubeEntity, crossSprite);
    world.emplace<UIZLevel>(cubeEntity, 1);

    // create move widget
    auto scaledX = 0.3f;
    auto scaledY = scaledX * context.backend.SwapchainAspect();

    auto lookWidget = world.create();
    world.emplace<UIRect>(lookWidget, math::vec2{0.0f, 0.0f}, math::vec2{1.0f, 1.0f});
    world.emplace<UIZLevel>(lookWidget, 0);
    world.emplace<UIRaycastTarget>(lookWidget);

    auto mvWidget = world.create();
    world.emplace<UIRect>(mvWidget, math::vec2{0.0f, 1.f - scaledY}, math::vec2{scaledX, scaledY});
    world.emplace<UIZLevel>(mvWidget, 1);
    world.emplace<UIRaycastTarget>(mvWidget);

    world.emplace<InputSourceWidget>(player, mvWidget, lookWidget);
}

void DebugScene3::Exit(PresentationContext& context) {}

void DebugScene3::Update(PresentationContext& context, const FrameInputData& frame, FrameOutputData& frameOut)
{
    auto& blocks = m_gameWorld.Get();

    m_gameUpdater.Update(m_gameWorld.Get(), context, frame);
    m_gameRenderer.Present(m_gameWorld.Get(), context, frameOut);

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
        if (gworld.all_of<ECS::InputSourceKeyMouse>(pe)) gworld.erase<ECS::InputSourceKeyMouse>(pe);
    }
    math::vec2 pos;
    if (frame.input.IsKeyReleased(KeyCode::KY_T) || UI::IsButtonClicked(m_gameWorld.Get(), m_terminalButton, pos))
    {
        if (m_gameWorld.Get().view<ECS::UITerminal>().empty())
        {
            UI::CreateTerminalPanel(m_gameWorld.Get(), context, {math::vec2{0.f, 0.f}, math::vec2{1.f, 1.f}});
        }
        else
        {
            auto e = m_gameWorld.Get().view<ECS::UITerminal>().front();
            m_gameWorld.Get().destroy(e);
        }
    }
}
}  // namespace OneGame
