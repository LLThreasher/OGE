#pragma once

#include <string>
#include <vector>

#include "game/json.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/defs.hpp"
#include "oge/runtime/oge_registry.hpp"
#include "oge/runtime/type_name.hpp"

namespace game
{

using oge::runtime::oge_id_type;
using BlockEntry =
    std::tuple<std::pmr::string, ::game::terrain::BlockConfig>;

struct SceneConfig
{
    static constexpr uint32_t LOAD_MASK_BLOCKS = 1 << 0;
    static constexpr uint32_t LOAD_MASK_TERRAIN = 1 << 1;

    uint32_t loadMask = 0;
    std::pmr::vector<BlockEntry> blocks;
    terrain::TerrainDesc terrainDesc;
    std::pmr::vector<oge_id_type> subsystems;
    std::pmr::vector<oge_id_type> realtimeSubsystems;

    bool empty() const
    {
        return loadMask == 0 && blocks.empty() && subsystems.empty() &&
               realtimeSubsystems.empty();
    }
    void clear()
    {
        loadMask = 0; blocks.clear(); terrainDesc = {};
        subsystems.clear(); realtimeSubsystems.clear();
    }
};

// =========================================================================
// GameWorld  —  OgeRegistry + entity event recording
//
// Inherits all of OgeRegistry's safe accessors.  Adds an optional
// EntityEventStream for networked / authoritative mode.  Call
// EnableEntityEvents() before create/destroy to record events.
// =========================================================================

class GameWorld : public oge::runtime::OgeRegistry
{
   public:
    using OgeRegistry::Entity;

    GameWorld() = default;
    NO_COPY(GameWorld)

    template <typename T, typename Fn>
    auto patch(Entity e, Fn fn)
    {
        return Raw().patch<T>(e, fn);
    }

    template <typename T>
    auto patch(Entity e)
    {
        return Raw().patch<T>(e);
    }
};

// =========================================================================
// AuthoritativeGameWorld  —  convenience alias for server-side worlds
// =========================================================================

using AuthoritativeGameWorld = GameWorld;

}  // namespace game

DECL_JSON_OBJ(::game::BlockEntry, {
    visit("id", std::get<std::pmr::string>(self));
    visit("config", std::get<::game::terrain::BlockConfig>(self));
})

DECL_JSON_OBJ(::game::terrain::TerrainDesc, {
    visit("chunk_view_distance", self.chunkViewDistance);
    visit("terrain_gen_chunk_budget", self.terrainGenChunkBudget);
})

DECL_JSON_OBJ(::game::SceneConfig, {
    visit("load_mask", self.loadMask);
    visit("blocks", self.blocks);
    visit("terrain_desc", self.terrainDesc);
    visit("subsystems", self.subsystems);
    visit("realtime_subsystems", self.realtimeSubsystems);
})
