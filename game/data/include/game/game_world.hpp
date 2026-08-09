#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "game/input/entity_event_stream.hpp"
#include "game/json.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/defs.hpp"
#include "oge/runtime/oge_registry.hpp"
#include "oge/runtime/typed_registry.hpp"

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

    /// Enable entity event recording (networked / authoritative mode).
    void EnableEntityEvents()
    {
        m_entityEventStream =
            &Raw().ctx().template emplace<input::EntityEventStream>();
    }
    bool HasEntityEvents() const { return m_entityEventStream != nullptr; }

    // -- entity lifecycle with optional event recording -------------------

    Entity create()
    {
        auto res = OgeRegistry::create();
        if (m_entityEventStream)
        {
            m_entityEventStream->Push(
                {input::EntityEventType::Create, res});
            m_entityEventStream->AdvanceCursor(m_eventCursor);
        }
        return res;
    }

    Entity create(Entity hint)
    {
        auto res = OgeRegistry::create(hint);
        if (m_entityEventStream)
        {
            m_entityEventStream->Push(
                {input::EntityEventType::Create, res});
            m_entityEventStream->AdvanceCursor(m_eventCursor);
        }
        return res;
    }

    void destroy(Entity e)
    {
        OgeRegistry::destroy(e);
        if (m_entityEventStream)
        {
            m_entityEventStream->Push(
                {input::EntityEventType::Destroy, e});
            m_entityEventStream->AdvanceCursor(m_eventCursor);
        }
    }

    void ApplyEvents()
    {
        if (!m_entityEventStream) return;
        input::EntityEvent event;
        while (m_entityEventStream->PollOne(m_eventCursor, event))
        {
            if (event.type == input::EntityEventType::Create)
                OgeRegistry::create(event.entity);
            else if (event.type == input::EntityEventType::Destroy)
                OgeRegistry::destroy(event.entity);
        }
    }

   private:
    input::EntityEventStream* m_entityEventStream = nullptr;
    input::EntityEventStream::Cursor m_eventCursor;
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
