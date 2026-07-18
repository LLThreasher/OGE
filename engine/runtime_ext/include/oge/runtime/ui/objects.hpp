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

}  // namespace OneGame::Engine
