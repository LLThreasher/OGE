#pragma once

#include "oge/runtime/entt.hpp"
#include "oge/runtime/ui/objects.hpp"
#include "oge/math.hpp"

namespace game::math
{
    using namespace oge::math;
}

namespace game::ui
{
using namespace oge::runtime::ui;

using oge::runtime::AssetContext;

entt::entity CreateGameView(entt::registry& game, UIRect rect);
entt::entity CreateTerminalPanel(entt::registry& game, AssetContext& asset, UIRect rect);
entt::entity CreateButton(entt::registry& game, AssetContext& asset, UIRect rect);

struct UICursor
{
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

struct UIDragReleaseFinished
{
    entt::entity dragDst;
};

struct UIDragReleaseDst
{
    entt::entity dragStart;

    const UIDrag& GetDrag(const entt::registry& world) const;
};

struct UIDragReleaseInfo
{
    UIDrag& drag;
    entt::entity start;
    entt::entity end;
};

bool IsButtonClicked(const entt::registry& game, entt::entity button);
bool IsButtonClicked(const entt::registry& game, entt::entity button, math::vec2& clickPos);
bool IsDragReleasedSrc(const entt::registry& game, entt::entity src);
std::tuple<const UIDrag*, entt::entity> TryGetReleasedDragSrc(const entt::registry& game, entt::entity e);
std::tuple<const UIDrag*, entt::entity> TryGetReleasedDragDst(const entt::registry& game, entt::entity e);
}  // namespace game::ui
