#pragma once

#include <vector>

#include "game/input/entity_event_stream.hpp"
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
        visit(self.subsystems);
        visit(self.realtimeSubsystems);
    }

    bool empty()
    {
        return subsystems.empty() && realtimeSubsystems.empty();
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
    GameWorld(entt::registry& world) : m_world(world), m_entityEventStream(m_world.ctx().emplace<input::EntityEventStream>())
    {
    }

    template<typename T, typename... Args>
    auto view()
    {
        return m_world.view<T, Args...>();
    }

    auto ctx()
    {
        return m_world.ctx();
    }

    template<typename T>
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

    template<typename T, typename... Args>
    auto emplace(Entity e, Args... args)
    {
        return m_world.emplace<T>(e, args...);
    }
};

}  // namespace game
