#pragma once

#include <vector>
#include "oge/runtime/net_serializer.hpp"
#include "game/terrain/defs.hpp"
#include "oge/runtime/typed_registry.hpp"
#include "oge/runtime/entt.hpp"

namespace game {
using oge::runtime::oge_id_type;
namespace net = oge::runtime::net;

NET_OBJ(GameWorldConfig)
{
    terrain::TerrainDesc terrainDesc;
    terrain::TerrainRendererDesc terrainRendererDesc;
    net::List<oge_id_type> subsystems;
    net::List<oge_id_type> renderers;

    NET_OBJ_FN
    {
        visit(terrainDesc);
        visit(terrainRendererDesc);
        visit(subsystems);
        visit(renderers);
    }
};

}
