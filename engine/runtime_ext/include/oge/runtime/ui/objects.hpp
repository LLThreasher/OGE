#pragma once

#include "oge/rect.hpp"
#include "oge/input/mouse.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/gfx/commands.hpp"
#include "oge/submission_group.hpp"

namespace oge::runtime::ui
{
using MouseButton = ::oge::input::MouseButton;

class IFont;

struct UIRect : FRect
{
};

struct ScreenRect : IRect16
{
};

struct UISprite
{
    PSprite sprite;
    ColorRGBA8 color = COLOR_WHITE;
};

struct UIText
{
    std::shared_ptr<IFont> font;
    std::string text = "";
    uint32_t size = 16;
    ColorRGBA8 color = COLOR_WHITE;
    bool enableWrap = false;
    bool enableCutoff = false;
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

struct UITerminal
{
    entt::entity text;
    int offset;
};

struct UITextInput
{
};

struct UIParent
{
    entt::entity parent;
};
}  // namespace OneGame::Engine::ECS

namespace oge::runtime
{
struct AssetContext;
class SubmissionQueue;
}
namespace oge::runtime::ui
{

class IFont
{
   public:
    virtual ~IFont() = default;
    virtual void CreateTextSprites(SubmissionView<gfx::CmdDrawSprite>& squeue, UIText text, ScreenRect rect) = 0;
};
std::unique_ptr<IFont> LoadASCIIBitmapFont16x6(AssetContext& ctx, std::string_view textureId);

math::vec2 ScreenSpaceToRelSpace(const ScreenRect rect, math::vec2 screenPos);
math::vec2 ScreenSpaceToRelSpace(const entt::registry& world, entt::entity rectEntity, math::vec2 screenPos);
math::vec2 ScreenSpaceToRelSpace(const entt::registry& world, math::vec2 screenPos);
Point2 RelSpaceToScreenSpace(const entt::registry& world, math::vec2 relPos);
ScreenRect UIRectToScreenRect(const entt::registry& world, entt::entity rect);
entt::entity CreateGameView(entt::registry& game, UIRect rect);
entt::entity CastRayScreenSpace(const entt::registry& gameWorld, math::vec2 pos);
entt::entity CreateTerminalPanel(entt::registry& game, AssetContext& asset, UIRect rect);
entt::entity CreateButton(entt::registry& game, AssetContext& asset, UIRect rect);
bool IsButtonClicked(const entt::registry& game, entt::entity button, math::vec2& clickPos);
bool IsDragReleasedSrc(const entt::registry& game, entt::entity src);
std::tuple<const UIDrag*, entt::entity> TryGetReleasedDragSrc(const entt::registry& game, entt::entity e);
std::tuple<const UIDrag*, entt::entity> TryGetReleasedDragDst(const entt::registry& game, entt::entity e);

}  // namespace OneGame::Engine
