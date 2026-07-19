#include "oge/log.hpp"
#include "oge/runtime/ui/objects.hpp"
#include "game/ui/objects.hpp"
#include "oge/runtime/asset_ctx.hpp"
#include "game/view/submission_queue.hpp"

namespace game::ui
{
using namespace oge;

entt::entity CreateButton(entt::registry& game, AssetContext& asset, UIRect rect)
{
    auto res = game.create();
    game.emplace<UIRect>(res, rect);
    game.emplace<UIZLevel>(res, 3);
    game.emplace<UIRaycastTarget>(res);
    return res;
}

entt::entity CreateTerminalPanel(entt::registry& game, AssetContext& asset, UIRect rect)
{
    auto view = game.create();
    auto res = game.create();
    game.emplace<UITerminal>(res, view);
    game.emplace<UIRect>(res, rect);
    game.emplace<UISprite>(res, UISprite{.sprite = asset.LoadTexture("invalid.png"), .color = COLOR_BLACK});
    game.emplace<UIZLevel>(res, 2);
    game.emplace<UIRaycastTarget>(res);

    game.emplace<UIParent>(view, res);
    game.emplace<UIRect>(view, UIRect{math::vec2{0.f, 0.f}, math::vec2{1.f, 1.f}});
    game.emplace<UIText>(view,
                         UIText{.font = asset.LoadASCIIBitmapFont16x6("om_tall_plain_idx.png"), .text = "Terminal"});
    return res;
}

entt::entity CreateGameView(entt::registry& game, const UIRect rect)
{
    std::unordered_set freeSlots{
        view::GameViewType::Slot0,
        view::GameViewType::Slot1,
        view::GameViewType::Slot2,
        view::GameViewType::Slot3,
    };
    for (auto [entity, view] : game.view<view::ViewPanel>().each())
    {
        freeSlots.erase(view.activeSlot);
    }
    if (freeSlots.empty())
    {
        assert(false && "too many view panels");
    }
    auto res = game.create();
    game.emplace<view::ViewPanel>(res).activeSlot = *freeSlots.begin();
    game.emplace<UIRect>(res, rect);
    LOG_INFO("game view created at slot {}", static_cast<int>(game.get<view::ViewPanel>(res).activeSlot));
    return res;
}

} // namespace oge::runtime::ui
