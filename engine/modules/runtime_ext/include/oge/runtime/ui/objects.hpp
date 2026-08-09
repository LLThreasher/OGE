#pragma once

#include <string_view>

#include "oge/color.hpp"
#include "oge/input/mouse.hpp"
#include "oge/rect.hpp"
#include "oge/runtime/type_name.hpp"
#include "oge/runtime/oge_registry.hpp"
#include "oge/submission_group.hpp"
#include "oge/runtime/objects_ext.hpp"


namespace oge::runtime::ui
{
using namespace oge::colors;
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
    ColorRGBA8 color = WHITE;
};

enum class TextAlignment : uint8_t
{
    Left = 0,
    Right,
};

struct UITextData
{
    std::string_view text = "";
    uint32_t size = 16;
    ColorRGBA8 color = WHITE;
    bool enableWrap = false;
    bool enableCutoff = false;
    TextAlignment alignment;
};

struct UIText
{
    std::shared_ptr<IFont> font;
    UITextData data;
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
}  // namespace oge::runtime::ui

namespace oge::runtime
{
struct AssetContext;
class SubmissionQueue;
}  // namespace oge::runtime
namespace oge::runtime::ui
{

class IFont
{
   public:
    virtual ~IFont() = default;
    virtual void CreateTextSprites(SubmissionView<CmdDrawSprite> squeue,
                                   UITextData text, ScreenRect rect) = 0;
};
std::unique_ptr<IFont> LoadASCIIBitmapFontMxN(int m, int n, AssetContext& ctx,
                                              std::string_view textureId);

math::vec2 ScreenSpaceToRelSpace(const ScreenRect rect, math::vec2 screenPos);
math::vec2 ScreenSpaceToRelSpace(const OgeRegistryRef world,
                                 entt::entity rectEntity, math::vec2 screenPos);
math::vec2 ScreenSpaceToRelSpace(const OgeRegistryRef world,
                                 math::vec2 screenPos);
Point2 RelSpaceToScreenSpace(const OgeRegistryRef world, math::vec2 relPos);
ScreenRect UIRectToScreenRect(const OgeRegistryRef world, entt::entity rect);
void UpdateUIRectToScreenRect(OgeRegistryRef world, entt::entity rect);

entt::entity CastRayRelSpace(const OgeRegistryRef gameWorld, math::vec2 pos);
entt::entity CastRayScreenSpace(const OgeRegistryRef gameWorld,
                                math::vec2 pos);

}  // namespace oge::runtime::ui

DECL_TYPE_NAME(oge::runtime::ui::UIRect, "core::UIRect")
DECL_TYPE_NAME(oge::runtime::ui::UISprite, "core::UISprite")
DECL_TYPE_NAME(oge::runtime::ui::UIZLevel, "core::UIZLevel")
DECL_TYPE_NAME(oge::runtime::ui::UIRaycastTarget, "core::UIRaycastTarget")

DECL_TYPE_NAME(oge::runtime::ui::ScreenRect, "core::ScreenRect")
DECL_TYPE_NAME(oge::runtime::ui::UITerminal, "core::UITerminal")
DECL_TYPE_NAME(oge::runtime::ui::UIText, "core::UIText")
DECL_TYPE_NAME(oge::runtime::ui::UIParent, "core::UIParent")
DECL_TYPE_NAME(oge::runtime::ui::UIRoot, "core::UIRoot")
