#pragma once

#include "oge/runtime/entt.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::runtime::AnythingFactory;
struct AppContext
{
    AnythingFactory& any_factory;
    entt::dispatcher& events;
};
}  // namespace game
