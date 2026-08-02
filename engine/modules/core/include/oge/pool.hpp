#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "oge/handle.hpp"

namespace oge
{
template <auto Tag, typename Resource>
class Pool
{
   public:
    using Handle = Handle<Tag>;

   private:
    struct Entry
    {
        bool alive = false;
        uint16_t generation = 1;
        alignas(Resource) std::byte storage[sizeof(Resource)];

        Resource* Get()
        {
            return std::launder(reinterpret_cast<Resource*>(storage));
        }

        const Resource* Get() const
        {
            return std::launder(reinterpret_cast<const Resource*>(storage));
        }
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
        uint16_t index;

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

        return Handle{++index, entry.generation};
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
    size_t Size() const noexcept
    {
        return m_entries.size() - m_freeList.size();
    }

    size_t Capacity() const noexcept
    {
        return m_entries.size();
    }

    void Clear()
    {
        m_entries.clear();
        m_freeList.clear();
    }

    Handle Poll() const
    {
        Handle res{};
        Poll(res);
        return res;
    }

    Resource* Poll(Handle& cursor)
    {
        ++cursor.index;
        size_t size = m_entries.size() + 1;
        for (; cursor.index < size; ++cursor.index)
        {
            Entry& entry = m_entries[cursor.index - 1];
            cursor.generation = entry.generation;

            if (entry.alive)
            {
                return entry.Get();
            }
        }
        return nullptr;
    }

    const Resource* Poll(Handle& cursor) const
    {
        ++cursor.index;
        size_t size = m_entries.size() + 1;
        for (; cursor.index < size; ++cursor.index)
        {
            const Entry& entry = m_entries[cursor.index - 1];
            cursor.generation = entry.generation;

            if (entry.alive)
            {
                return entry.Get();
            }
        }
        return nullptr;
    }
};
}  // namespace oge
