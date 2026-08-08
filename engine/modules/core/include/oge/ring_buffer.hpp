#pragma once

#include <stddef.h>

#include <cinttypes>
#include <concepts>
#include <memory>
#include <tuple>
#include <utility>

#include "oge/log.hpp"

namespace oge
{

template <typename T, size_t Capacity>
class RingBuffer
{
   public:
    using Index = uint64_t;

    constexpr static size_t capacity = Capacity;

    void Push(const T& value)
    {
        m_buffer[m_head % Capacity] = value;
        ++m_head;
    }

    template <typename... Args>
        requires std::constructible_from<T, Args...>
    void EmplaceBack(Args&&... args)
    {
        m_buffer[m_head % Capacity] = T(std::forward<Args>(args)...);
        ++m_head;
    }

    bool Contains(Index index) const
    {
        const Index begin = m_head > Capacity ? m_head - Capacity : 0;
        const Index end = m_head;

        return begin <= index && index < end;
    }

    const T& Get(Index index) const
    {
        // OGE_ASSERT(Contains(index), "RingBuffer index out of range");
        if (index == 0) index = m_head - 1;
        return m_buffer[index % Capacity];
    }

    T& Get(Index index)
    {
        // OGE_ASSERT(Contains(index), "RingBuffer index out of range");
        if (index == 0) index = m_head - 1;
        return m_buffer[index % Capacity];
    }

    const T& Head() const
    {
        // OGE_ASSERT(m_head > 0, "RingBuffer is empty");
        return m_buffer[(m_head - 1) % Capacity];
    }

    T& Head()
    {
        // OGE_ASSERT(m_head > 0, "RingBuffer is empty");
        return m_buffer[(m_head - 1) % Capacity];
    }

    Index HeadCursor() const
    {
        return m_head;
    }

    Index CurrentCursor() const
    {
        return m_head - 1;
    }

   protected:
    T m_buffer[Capacity] = {};
    Index m_head = 1;
};

}  // namespace oge
