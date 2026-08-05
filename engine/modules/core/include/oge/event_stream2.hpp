#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

template <typename T, std::size_t Capacity = 256>
class NetworkEventStream
{
public:
    using TEvent = T;
    using Cursor = std::uint64_t;

    static constexpr std::size_t MCapacity = Capacity;

private:
    struct Slot
    {
        Cursor sequence = 0;
        bool occupied = false;
        TEvent event{};
    };

    std::array<Slot, Capacity> m_slots{};

    // One-past-highest contiguous/appended event index for normal Push().
    Cursor m_head = 1;

    // Highest sequence ever inserted/pushed, plus one.
    Cursor m_frontier = 1;

public:
    NetworkEventStream() = default;

    void Clear()
    {
        m_slots = {};
        m_head = 1;
        m_frontier = 1;
    }

    void Push(const TEvent& event)
    {
        Insert(m_head, event);
        ++m_head;

        if (m_head > m_frontier)
        {
            m_frontier = m_head;
        }
    }

    bool Insert(Cursor sequence, const TEvent& event)
    {
        if (sequence == 0)
        {
            return false;
        }

        // Too old to be safely represented.
        if (m_frontier > Capacity && sequence + Capacity < m_frontier)
        {
            return false;
        }

        Slot& slot = m_slots[sequence % Capacity];

        // Duplicate insert.
        if (slot.occupied && slot.sequence == sequence)
        {
            return false;
        }

        // Overwrite slot.
        slot.sequence = sequence;
        slot.occupied = true;
        slot.event = event;

        if (sequence >= m_frontier)
        {
            m_frontier = sequence + 1;
        }

        if (sequence >= m_head)
        {
            m_head = sequence + 1;
        }

        return true;
    }

    bool PollOne(Cursor& cursor, TEvent& output) const
    {
        return PollOne(cursor, output, m_frontier);
    }

    bool PollOne(Cursor& cursor, TEvent& output, Cursor frontier) const
    {
        if (cursor == 0)
        {
            cursor = frontier;
        }

        if (cursor >= frontier)
        {
            return false;
        }

        const Slot& slot = m_slots[cursor % Capacity];

        if (!slot.occupied || slot.sequence != cursor)
        {
            return false;
        }

        output = slot.event;
        ++cursor;
        return true;
    }

    void AdvanceCursor(Cursor& cursor) const
    {
        cursor = m_frontier;
    }

    bool Has(Cursor sequence) const
    {
        if (sequence == 0)
        {
            return false;
        }

        const Slot& slot = m_slots[sequence % Capacity];

        return slot.occupied && slot.sequence == sequence;
    }

    bool TryGet(Cursor sequence, TEvent& output) const
    {
        if (!Has(sequence))
        {
            return false;
        }

        output = m_slots[sequence % Capacity].event;
        return true;
    }

    Cursor HeadIndex() const
    {
        return m_head;
    }

    Cursor Frontier() const
    {
        return m_frontier;
    }

    Cursor OldestPossibleIndex() const
    {
        if (m_frontier <= Capacity)
        {
            return 1;
        }

        return m_frontier - Capacity;
    }
};
