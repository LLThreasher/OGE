#pragma once

#include <stddef.h>

#include <cinttypes>

#include "oge/log.hpp"

using EventStreamCursor = uint64_t;

namespace oge
{
template <typename T, size_t bufferSize = 256>
class DiscreteEventStream
{
   public:
    using Cursor = EventStreamCursor;

    bool PollOne(Cursor& cursor, T& output) const { return PollOne(cursor, output, m_currentHead); }

    bool PollOne(Cursor& cursor, T& output, const Cursor& frontier) const
    {
        if (cursor == 0) AdvanceCursor(cursor);
        if (cursor >= frontier) return false;
        if (m_currentHead - cursor > bufferSize)
            LOG_WARN("stale cursor detected at {}, current head {}", cursor, m_currentHead);
        output = m_ringBuffer[(cursor++) % bufferSize];
        return true;
    }

    T Head() const { return m_ringBuffer[(m_currentHead - 1) % bufferSize]; }

    T Get(const Cursor& cursor) const { return m_ringBuffer[cursor % bufferSize]; }

    void AdvanceCursor(Cursor& cursor) const { cursor = m_currentHead; }

    void Push(T delta) { m_ringBuffer[(m_currentHead++) % bufferSize] = delta; }

   protected:
    T m_ringBuffer[bufferSize] = {};
    Cursor m_currentHead = 2;
};

template <typename T>
concept IsAccumulativeEvent = requires(T a, T b) {
    std::is_default_constructible_v<T>;
    { a - b };
};

template <typename T, size_t bufferSize = 256>
    requires IsAccumulativeEvent<T>
class AccumulativeEventStream : public DiscreteEventStream<T, bufferSize>
{
   public:
    using Cursor = EventStreamCursor;
    T PollDelta(Cursor& cursor) const { return PollDelta(cursor, this->m_currentHead); }

    T PollDelta(Cursor& cursor, const Cursor& frontier) const
    {
        if (cursor == 0)
            this->AdvanceCursor(cursor);
        if (cursor >= frontier) return {};
        if (this->m_currentHead - cursor > bufferSize)
            LOG_WARN("stale cursor detected at {}, current head {}", cursor, this->m_currentHead);
        auto res = this->m_ringBuffer[(frontier - 1) % bufferSize] - this->m_ringBuffer[(cursor - 1) % bufferSize];
        cursor = frontier;
        return res;
    }
};

enum class EventStatus : uint8_t
{
    Start = 0,
    Update,
    End,
};

struct CompositeEvent
{
    EventStatus status = EventStatus::Start;
    char channel = -1;
};

template <typename T, typename TDelta, size_t channelSize = 16, size_t bufferSize = 256>
    requires std::is_default_constructible_v<T> && std::derived_from<T, CompositeEvent> && requires(T a, TDelta b) {
        { T::Update(a, b) } -> std::same_as<T>;
    }
class CompositeEventStream
{
   public:
    using Cursor = std::array<EventStreamCursor, channelSize>;

    bool PollOne(size_t channel, Cursor& cursor, T& event)
    {
        auto& stream = m_channels[channel];
        auto& ccursor = cursor[channel];
        if (!stream.PollOne(ccursor, event)) return false;
        assert(event.status == EventStatus::Start);
        while (stream.PollOne(ccursor, event))
        {
            if (event.status == EventStatus::End)
            {
                cursor[channel] = ccursor;
                return true;
            }
        }
        return true;
    }

    bool PollOne(Cursor& cursor, T& event)
    {
        for (size_t i = 0; i < channelSize; ++i)
        {
            if (PollOne(i, cursor, event)) return true;
        }
        return false;
    }

    void PushStart(size_t channel, T data)
    {
        auto& c = m_channels[channel];
        assert(data.status == EventStatus::Start);
        assert(c.Head().status == EventStatus::End);
        data.channel = channel;
        c.Push(data);
    }

    void PushDelta(size_t channel, TDelta delta, EventStatus status)
    {
        auto& c = m_channels[channel];
        assert(c.Head().status != EventStatus::Start);
        auto event = T::Update(c.Head(), delta);
        event.status = status;
        event.channel = channel;
        c.Push(event);
    }

   private:
    std::array<DiscreteEventStream<T, bufferSize>, channelSize> m_channels;
};

}  // namespace oge
