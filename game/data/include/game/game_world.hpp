#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "game/input/entity_event_stream.hpp"
#include "game/json.hpp"
#include "game/terrain/block_registry.hpp"
#include "game/terrain/defs.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::runtime::oge_id_type;

using BlockEntry = std::tuple<std::pmr::string, ::game::terrain::BlockConfig>;

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
        loadMask = 0;
        blocks.clear();
        terrainDesc = {};
        subsystems.clear();
        realtimeSubsystems.clear();
    }
};

class GameWorld
{
   protected:
    entt::registry& m_world;
    input::EntityEventStream& m_entityEventStream;
    input::EntityEventStream::Cursor m_eventCursor;

   public:
    using Entity = entt::entity;
    GameWorld(entt::registry& world)
        : m_world(world),
          m_entityEventStream(m_world.ctx().emplace<input::EntityEventStream>())
    {
    }

    template <typename T, typename... Args>
    auto view()
    {
        return m_world.view<T, Args...>();
    }

    auto ctx()
    {
        return m_world.ctx();
    }

    template <typename T>
    auto get(const Entity e)
    {
        return m_world.get<T>(e);
    }

    void ApplyEvents()
    {
        input::EntityEvent event;
        while (m_entityEventStream.PollOne(m_eventCursor, event))
        {
            if (event.type == input::EntityEventType::Create)
                auto _ = m_world.create(event.entity);
            else if (event.type == input::EntityEventType::Destroy)
                m_world.destroy(event.entity);
        }
    }
};

class AuthoritativeGameWorld : public GameWorld
{
   public:
    Entity create()
    {
        auto res = m_world.create();
        m_entityEventStream.Push({input::EntityEventType::Create, res});
        m_entityEventStream.AdvanceCursor(m_eventCursor);
        return res;
    }

    void destroy(Entity e)
    {
        m_world.destroy(e);
        m_entityEventStream.Push({input::EntityEventType::Destroy, e});
        m_entityEventStream.AdvanceCursor(m_eventCursor);
    }

    template <typename T, typename... Args>
    auto emplace(Entity e, Args... args)
    {
        return m_world.emplace<T>(e, args...);
    }
};

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
