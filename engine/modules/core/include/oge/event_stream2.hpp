#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace oge
{

template <typename T, std::size_t Capacity = 256>
class NetworkEventStream
{
   public:
    using Event = T;
    using Cursor = uint32_t;

    static constexpr std::size_t MCapacity = Capacity;

   private:
    struct Slot
    {
        Cursor sequence = 0;
        bool occupied = false;
        Event event{};
    };

    std::array<Slot, Capacity> m_slots{};

    // Next sequence assigned by Insert(event).
    Cursor m_head = 1;

    // Current stream tick / readable frontier.
    //
    // Events with sequence < m_tick are considered available to Peek/Poll.
    // Insert(sequence, event) can place future events, but they only become
    // readable after AdvanceTick moves m_tick past them.
    Cursor m_tick = 1;

   public:
    NetworkEventStream() = default;

    void Clear()
    {
        m_slots = {};
        m_head = 1;
        m_tick = 1;
    }

    // Appends at the next local head sequence.
    Cursor Insert(const Event& event)
    {
        const Cursor sequence = m_head;
        Insert(sequence, event);
        ++m_head;
        return sequence;
    }

    // Inserts at an explicit sequence.
    bool Insert(Cursor sequence, const Event& event)
    {
        if (sequence == 0)
        {
            Insert(event);
            return true;
        }

        // Too old to still fit in the ring.
        if (m_tick > Capacity && sequence + Capacity < m_tick)
        {
            return false;
        }

        Slot& slot = m_slots[sequence % Capacity];

        // Duplicate insert.
        if (slot.occupied && slot.sequence == sequence)
        {
            return false;
        }

        slot.sequence = sequence;
        slot.occupied = true;
        slot.event = event;

        if (sequence >= m_head)
        {
            m_head = sequence + 1;
        }

        return true;
    }

    // Compatibility alias.
    void Push(const Event& event)
    {
        Insert(event);
    }

    // Reads the event at cursor without advancing cursor.
    bool Peek(Cursor cursor, Event& output) const
    {
        if (cursor == 0)
        {
            return false;
        }

        if (cursor >= m_tick)
        {
            return false;
        }

        const Slot& slot = m_slots[cursor % Capacity];

        if (!slot.occupied || slot.sequence != cursor)
        {
            return false;
        }

        output = slot.event;
        return true;
    }

    // Polls the event at cursor and advances cursor by one only on success.
    bool Poll(Cursor& cursor, Event& output) const
    {
        if (cursor == 0)
        {
            cursor = OldestPossibleIndex();
        }

        if (!Peek(cursor, output))
        {
            return false;
        }

        ++cursor;
        return true;
    }

    // Compatibility alias.
    bool PollOne(Cursor& cursor, Event& output) const
    {
        return Poll(cursor, output);
    }

    // Compatibility overload with explicit frontier.
    bool PollOne(Cursor& cursor, Event& output, Cursor frontier) const
    {
        if (cursor == 0)
        {
            cursor = OldestPossibleIndex(frontier);
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

    // Advances the stream tick/frontier by one.
    void AdvanceTick()
    {
        ++m_tick;
    }

    // Advances the stream tick/frontier to at least target_tick.
    void AdvanceTick(Cursor target_tick)
    {
        if (target_tick > m_tick)
        {
            m_tick = target_tick;
        }
    }

    // Compatibility helper.
    void AdvanceCursor(Cursor& cursor) const
    {
        cursor = m_tick;
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

    bool TryGet(Cursor sequence, Event& output) const
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

    Cursor Tick() const
    {
        return m_tick;
    }

    // Compatibility name.
    Cursor Frontier() const
    {
        return m_tick;
    }

    Cursor OldestPossibleIndex() const
    {
        return OldestPossibleIndex(m_tick);
    }

    static Cursor OldestPossibleIndex(Cursor frontier)
    {
        if (frontier <= Capacity)
        {
            return 1;
        }

        return frontier - Capacity;
    }
};

}  // namespace oge
