#pragma once

#include <vector>

#include "game/terrain/defs.hpp"
#include "oge/runtime/entt.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::runtime::oge_id_type;
namespace net = oge::runtime::net;

NET_OBJ(SceneConfig)
{
    net::List<oge_id_type> subsystems;
    net::List<oge_id_type> realtimeSubsystems;

    NET_OBJ_FN
    {
        visit(subsystems);
        visit(realtimeSubsystems);
    }
};

}  // namespace game
