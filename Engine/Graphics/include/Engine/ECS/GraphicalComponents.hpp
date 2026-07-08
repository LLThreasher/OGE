#pragma once
#include "Engine/entt.hpp"
#include "Engine/Rect.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"

namespace OneGame::Engine::ECS
{

struct UIRect : FRect
{
};

struct ScreenRect : IRect16
{
};

struct UISprite
{
    Graphics::PSprite sprite;
    ColorRGBA8 color = COLOR_WHITE;
};

struct UIText
{
    std::string text;
};

struct UIDrag
{
    int inputIndex = -1;
    MouseButton dragStartButton = MouseButton::Left;
    entt::entity onTopOf = entt::null;
    math::vec2 dragStartPos;
    math::vec2 dragLastPos;
    float deltaTime = 0.f;
    math::vec2 dragDelta = {};
    math::vec2 maxDragDelta = {};

    void UpdateDrag(math::vec2 pos, entt::entity onTopOf, float dt);
    bool IsHold(const entt::registry& world, int pixelRadiusSqr = 200) const;
    bool IsClick(const entt::registry& world, float duration = 0.25f, int pixelRadiusSqr = 200) const;
};

struct UIDragRelease
{
    UIDrag drag;
    entt::entity dragStart;
};

struct UIZLevel
{
    int zLevel = 0;
};

struct UIRaycastTarget
{
};

struct UIRaycastHit
{
};

struct UIRoot
{
};

struct SwapchainExtent : UPoint2
{
};

struct UIParent
{
    entt::entity parent;
};

struct ViewPanel
{
    Graphics::GameViewType activeSlot = Graphics::GameViewType::Slot0;
    entt::entity activeCamera = entt::null;
};
}

namespace OneGame::Engine::UI
{
    using namespace ECS;
    math::vec2 ScreenSpaceToRelSpace(const ScreenRect rect, math::vec2 screenPos);
    math::vec2 ScreenSpaceToRelSpace(const entt::registry& world, entt::entity rectEntity,
                                     math::vec2 screenPos);
    math::vec2 ScreenSpaceToRelSpace(const entt::registry& world, math::vec2 screenPos);
    Point2 RelSpaceToScreenSpace(const entt::registry& world, math::vec2 relPos);
    ScreenRect UIRectToScreenRect(const entt::registry& world, entt::entity rect);
    entt::entity CreateGameView(entt::registry & game, UIRect rect);
    entt::entity CastRayScreenSpace(const entt::registry& gameWorld, math::vec2 pos);
}
