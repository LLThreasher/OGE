#pragma once

#include <stddef.h>

#include <cinttypes>

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

    const T& Get(Index index) const
    {
        return m_buffer[index % Capacity];
    }

    const T& Head() const
    {
        return m_buffer[(m_head - 1) % Capacity];
    }

    Index HeadIndex() const
    {
        return m_head;
    }

protected:
    T m_buffer[Capacity] = {};
    Index m_head = 0;
};

}  // namespace oge
