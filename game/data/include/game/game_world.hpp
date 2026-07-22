#pragma once

#include "oge/runtime/net_serializer.hpp"
#include "game/terrain/defs.hpp"

namespace game {
namespace net = oge::runtime::net;
class DistrubutedGameWorld
{
    NET_OBJ(WorldConfig)
    {
        terrain::TerrainDesc terrainDesc;
        terrain::TerrainRendererDesc terrainRendererDesc;

        NET_OBJ_FN
        {
            visit(terrainDesc);
            visit(terrainRendererDesc);
        }
    };
};
}
