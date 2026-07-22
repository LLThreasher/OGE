#pragma once

#include <vector>
#include "oge/runtime/net_serializer.hpp"
#include "game/terrain/defs.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game {
namespace net = oge::runtime::net;
class DistrubutedGameWorld
{
};

NET_OBJ(SubsystemList), std::vector<oge::runtime::oge_id_type>
{
};

NET_OBJ(RendererList)
{
    std::vector<oge::runtime::oge_id_type> subsystems;
};

NET_OBJ(GameWorldConfig)
{
    terrain::TerrainDesc terrainDesc;
    terrain::TerrainRendererDesc terrainRendererDesc;

    NET_OBJ_FN
    {
        visit(terrainDesc);
        visit(terrainRendererDesc);
    }
};
}
