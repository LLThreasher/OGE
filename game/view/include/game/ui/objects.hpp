#pragma once

#include "oge/math.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/oge_registry.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "oge/runtime/ui/objects.hpp"


namespace game::ui
{
namespace math = ::oge::math;
using namespace oge::runtime::ui;

using oge::runtime::AssetContext;
using oge::runtime::OgeRegistryRef;

entt::entity CreateGameView(OgeRegistryRef game, UIRect rect,
                            entt::entity camera = entt::null);
entt::entity CreateTerminalPanel(OgeRegistryRef game, AssetContext& asset,
                                 UIRect rect);
entt::entity CreateButton(OgeRegistryRef game, AssetContext& asset,
                          UIRect rect);

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
    bool IsHold(const OgeRegistryRef world, int pixelRadiusSqr = 200) const;
    bool IsClick(const OgeRegistryRef world, float duration = 0.25f,
                 int pixelRadiusSqr = 200) const;
};

struct UIDragReleaseFinished
{
    entt::entity dragDst;
};

struct UIDragReleaseDst
{
    entt::entity dragStart;

    const UIDrag& GetDrag(const OgeRegistryRef world) const;
};

struct UIDragReleaseInfo
{
    UIDrag& drag;
    entt::entity start;
    entt::entity end;
};

bool IsButtonClicked(const OgeRegistryRef game, entt::entity button);
bool IsButtonClicked(const OgeRegistryRef game, entt::entity button,
                     math::vec2& clickPos);
bool IsDragReleasedSrc(const OgeRegistryRef game, entt::entity src);
std::tuple<const UIDrag*, entt::entity> TryGetReleasedDragSrc(
    const OgeRegistryRef game, entt::entity e);
std::tuple<const UIDrag*, entt::entity> TryGetReleasedDragDst(
    const OgeRegistryRef game, entt::entity e);
}  // namespace game::ui

DECL_TYPE_NAME(::game::ui::UIDragReleaseDst, "core::UIDragReleaseDst")
DECL_TYPE_NAME(::game::ui::UIDragReleaseFinished, "core::UIDragReleaseFinished")
DECL_TYPE_NAME(::game::ui::UIDrag, "core::UIDrag")
