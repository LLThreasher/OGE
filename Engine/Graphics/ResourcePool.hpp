#pragma once
#include <vector>
#include <cassert>

#include "ObjectType.hpp"


namespace OneGame::Engine
{
    template<ObjectType Tag, typename Resource>
    class ResourcePool
    {
    public:
        using Handle = ResourceHandle<Tag>;

    private:
        struct Entry
        {
            Resource resource;
            uint32_t generation = 1;
            bool alive = false;
        };

        std::vector<Entry>     m_entries;
        std::vector<uint32_t>  m_freeList;

    public:

        // ----------------------------
        // Create
        // ----------------------------
        template<typename... Args>
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

            entry.resource = Resource(std::forward<Args>(args)...);
            entry.alive = true;

            return Handle{ index + 1, entry.generation };
        }

        // ----------------------------
        // Destroy
        // ----------------------------
        void Destroy(Handle handle)
        {
            assert(IsAlive(handle));

            auto index = handle.index - 1;
            Entry& entry = m_entries[index];

            entry.alive = false;
            entry.generation++;           // invalidate old handles
            m_freeList.push_back(index);
        }

        // ----------------------------
        // Access
        // ----------------------------
        Resource& Get(Handle handle)
        {
            assert(IsAlive(handle));
            return m_entries[handle.index - 1].resource;
        }

        const Resource& Get(Handle handle) const
        {
            assert(IsAlive(handle));
            return m_entries[handle.index - 1].resource;
        }

        // ----------------------------
        // Validation
        // ----------------------------
        bool IsAlive(Handle handle) const
        {
            assert(handle.index != 0 && "using nullptr handle");
            auto index = handle.index - 1;
            if (index >= m_entries.size())
                return false;

            const Entry& entry = m_entries[index];

            return entry.alive &&
                entry.generation == handle.generation;
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

        Handle Poll()
        {
            assert(Size() > 0);
            for (uint32_t i = 0; i < m_entries.size(); ++i)
            {
                Entry& entry = m_entries[i];

                if (entry.alive)
                    return Handle{ i + 1, entry.generation };
            }
        }
    };
}
