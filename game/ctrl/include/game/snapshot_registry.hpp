#pragma once

#include "oge/runtime/typed_registry.hpp"

namespace game
{
using oge::runtime::FamilyId;
using oge::runtime::ICapability;
using oge::runtime::oge_id_type;
using oge::runtime::TypeRegistry;

struct SnapshotCapability : ICapability
{
    using CaptureFn = void (*)(entt::registry&, entt::any&);
    using RestoreFn = void (*)(entt::registry&, const entt::any&);
    using CreateStateFn = entt::any (*)();
    using HashFn = uint64_t (*)(entt::registry&);

    FamilyId family;

    CaptureFn capture = nullptr;
    RestoreFn restore = nullptr;
    CreateStateFn createState = nullptr;
    HashFn hash = nullptr;
};

class SnapshotRegistry
{
    struct TickState
    {
        uint32_t tick = 0;
        std::unordered_map<FamilyId, entt::any> state;
        uint64_t worldHash = 0;
    };

    std::unordered_map<FamilyId, SnapshotCapability*> m_units;
    std::deque<TickState> m_history;

    size_t m_maxHistory = 64;

public:

    void SetHistorySize(size_t size)
    {
        m_maxHistory = size;
    }

    void RegisterFrom(TypeRegistry& types)
    {
        for (auto& type : types.GetAll())
        {
            if (auto* cap = type.capabilities.Get<SnapshotCapability>())
            {
                m_units[cap->family] = cap;
            }
        }
    }

    void CaptureTick(entt::registry& world, uint32_t tick)
    {
        TickState ts;
        ts.tick = tick;

        uint64_t hash = 0;

        for (auto& [family, cap] : m_units)
        {
            if (!cap->createState || !cap->capture)
                continue;

            entt::any state = cap->createState();
            cap->capture(world, state);

            ts.state.emplace(family, std::move(state));

            if (cap->hash)
            {
                uint64_t h = cap->hash(world);
                hash ^= h + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
            }
        }

        ts.worldHash = hash;

        m_history.push_back(std::move(ts));

        if (m_history.size() > m_maxHistory)
            m_history.pop_front();
    }

    bool HasTick(uint32_t tick) const
    {
        for (auto& ts : m_history)
            if (ts.tick == tick)
                return true;

        return false;
    }

    uint64_t GetHash(uint32_t tick) const
    {
        for (auto& ts : m_history)
            if (ts.tick == tick)
                return ts.worldHash;

        return 0;
    }

    bool RestoreTick(entt::registry& world, uint32_t tick)
    {
        for (auto& ts : m_history)
        {
            if (ts.tick != tick)
                continue;

            for (auto& [family, state] : ts.state)
            {
                auto it = m_units.find(family);
                if (it == m_units.end())
                    continue;

                auto* cap = it->second;
                if (cap->restore)
                {
                    cap->restore(world, state);
                }
            }

            return true;
        }

        return false;
    }

    void Clear()
    {
        m_history.clear();
    }
};
}
