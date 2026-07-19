#include "oge/runtime/ui/objects.hpp"
#include "oge/runtime/entt.hpp"

namespace oge::runtime::ui
{

entt::entity CastRayScreenSpace(const entt::registry& gameWorld, math::vec2 pos)
{
    entt::entity resultEntity = entt::null;
    int maxZLevel = -1;
    for (auto [entity, rect, zLevel, srect] :
         gameWorld.view<UIRaycastTarget, const UIRect, const UIZLevel, const ScreenRect>().each())
    {
        if (srect.pos.x < pos.x && srect.pos.y < pos.y && pos.x < srect.pos.x + srect.extent.x &&
            pos.y < srect.pos.y + srect.extent.y)
        {
            if (zLevel.zLevel > maxZLevel)
            {
                maxZLevel = zLevel.zLevel;
                resultEntity = entity;
            }
        }
    }
    return resultEntity;
}

entt::entity CastRayRelSpace(const entt::registry& gameWorld, math::vec2 pos)
{
    return CastRayScreenSpace(gameWorld, RelSpaceToScreenSpace(gameWorld, pos));
}

ScreenRect UIRectToScreenRect(const entt::registry& world, entt::entity rect)
{
    if (auto sr = world.try_get<ScreenRect>(rect))
    {
        return *sr;
    }
    else
    {
        auto parent = world.try_get<UIParent>(rect);
        auto parentEntity = parent ? parent->parent : world.view<UIRoot>().front();
        ScreenRect srect = UIRectToScreenRect(world, parentEntity);
        if (auto _rect = world.try_get<UIRect>(rect))
        {
            srect.pos.x = (int32_t)(_rect->pos.x * srect.extent.x + srect.pos.x);
            srect.pos.y = (int32_t)(_rect->pos.y * srect.extent.y + srect.pos.y);
            srect.extent.x = (int32_t)(_rect->extent.x * srect.extent.x);
            srect.extent.y = (int32_t)(_rect->extent.y * srect.extent.y);
        }
        return srect;
    }
}

math::vec2 ScreenSpaceToRelSpace(const ScreenRect rect, math::vec2 screenPos)
{
    return (screenPos - static_cast<math::vec2>(rect.pos)) / static_cast<math::vec2>(rect.extent);
}

math::vec2 ScreenSpaceToRelSpace(const entt::registry& world, entt::entity rectEntity, math::vec2 screenPos)
{
    auto rect = UIRectToScreenRect(world, rectEntity);
    return ScreenSpaceToRelSpace(rect, screenPos);
}

math::vec2 ScreenSpaceToRelSpace(const entt::registry& world, math::vec2 screenPos)
{
    auto rectE = world.view<UIRoot>().front();
    return ScreenSpaceToRelSpace(world, rectE, screenPos);
}

Point2 RelSpaceToScreenSpace(const entt::registry& world, math::vec2 relPos)
{
    auto root = world.view<UIRoot>().front();
    auto rect = UIRectToScreenRect(world, root);
    return Point2::FromVec2(relPos * static_cast<math::vec2>(rect.extent) + static_cast<math::vec2>(rect.pos));
}
} // namespace oge::runtime::ui
