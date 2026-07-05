#pragma once
#include <cassert>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "Engine/ObjectType.hpp"

namespace OneGame::Engine
{
template <auto Tag, typename Resource>
class ResourcePool
{
   public:
    using Handle = ResourceHandle<Tag>;

   private:
    struct Entry
    {
        bool alive = false;
        uint32_t generation = 1;
        alignas(Resource) std::byte storage[sizeof(Resource)];

        Resource* Get() { return std::launder(reinterpret_cast<Resource*>(storage)); }

        const Resource* Get() const { return std::launder(reinterpret_cast<const Resource*>(storage)); }
    };

    std::vector<Entry> m_entries;
    std::vector<uint32_t> m_freeList;

   public:
    // ----------------------------
    // Create
    // ----------------------------
    template <typename... Args>
        requires std::constructible_from<Resource, Args...>
    Handle Create(Args&&... args)
    {
        uint32_t index;

        if (!m_freeList.empty())
        {
            index = m_freeList.back();
            m_freeList.pop_back();
        }
        else
        {
            index = static_cast<uint32_t>(m_entries.size());
            m_entries.emplace_back();
        }

        Entry& entry = m_entries[index];

        std::construct_at(entry.Get(), std::forward<Args>(args)...);

        entry.alive = true;

        return Handle{index + 1, entry.generation};
    }

    // ----------------------------
    // Destroy
    // ----------------------------
    void Destroy(Handle handle)
    {
        assert(IsAlive(handle));

        auto index = handle.index - 1;
        Entry& entry = m_entries[index];

        std::destroy_at(entry.Get());

        entry.alive = false;
        entry.generation++;

        m_freeList.push_back(index);
    }

    // ----------------------------
    // Access
    // ----------------------------
    Resource* Get(Handle handle)
    {
        if (!IsAlive(handle)) return nullptr;
        return m_entries[handle.index - 1].Get();
    }

    const Resource* Get(Handle handle) const
    {
        if (!IsAlive(handle)) return nullptr;
        return m_entries[handle.index - 1].Get();
    }

    // const Resource& Get(Handle handle) const
    //{
    //     assert(IsAlive(handle));
    //     return *m_entries[handle.index - 1].Get();
    // }

    // ----------------------------
    // Validation
    // ----------------------------
    bool IsAlive(Handle handle) const
    {
        if (handle.index == 0) return false;
        auto index = handle.index - 1;
        if (index >= m_entries.size()) return false;

        const Entry& entry = m_entries[index];

        return entry.alive && entry.generation == handle.generation;
    }

    // ----------------------------
    // Debug helpers
    // ----------------------------
    size_t Size() const noexcept { return m_entries.size() - m_freeList.size(); }

    size_t Capacity() const noexcept { return m_entries.size(); }

    void Clear()
    {
        m_entries.clear();
        m_freeList.clear();
    }

    Handle Poll()
    {
        assert(Size() > 0);
        for (uint32_t i = 0; i < m_entries.size(); ++i)
        {
            Entry& entry = m_entries[i];

            if (entry.alive) return Handle{i + 1, entry.generation};
        }
        return Handle::InvalidHandle();
    }
};
}  // namespace OneGame::Engine
