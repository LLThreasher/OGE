#pragma once

#include "game/net/replication_registry.hpp"

namespace game::net
{
template <typename TAdapter>
class LatestEventNetOutputStream
{
   public:
    using Adapter = TAdapter;
    using Stream = typename Adapter::Stream;
    using Cursor = typename Stream::Cursor;
    using Frame = typename Adapter::Frame;
    using Packet = typename Adapter::Packet;

   private:
    struct EncodeState
    {
        Cursor cursor{};

        /*
            Stream-local packet cursor.

            This is independent from Stream::Cursor.
        */
        NetCursor netCursor{0};
    };

    struct PlannedPacket
    {
        ReplicationTick tick = 0;
        NetCursor begin{};
        NetCursor end{};
        Packet packet{};
    };

    Stream& m_stream;

    mutable EncodeState m_encodeState{};
    mutable bool m_hasEncodeState = false;

   public:
    explicit LatestEventNetOutputStream(Stream& stream)
        : m_stream(stream)
    {
    }

    LatestEventNetOutputStream(const LatestEventNetOutputStream&) = delete;

    LatestEventNetOutputStream& operator=(
        const LatestEventNetOutputStream&) = delete;

    LatestEventNetOutputStream(LatestEventNetOutputStream&&) = delete;

    LatestEventNetOutputStream& operator=(
        LatestEventNetOutputStream&&) = delete;

    PacketPlan Peek(const EncodeContext& ectx) const
    {
        EncodeState state = GetOrCreateEncodeState();

        PlannedPacket planned{};
        if (!PeekNextFromState(m_stream, state, ectx, planned))
        {
            return {};
        }

        PacketPlan plan{};
        plan.hasPacket = true;
        plan.tick = planned.tick;
        plan.begin = planned.begin;
        plan.end = planned.end;
        plan.itemCount = 1;
        plan.byteCount = net::Size(planned.packet);

        return plan;
    }

    bool Poll(
        const EncodeContext& ectx,
        Packet& packet) const
    {
        EncodeState& state = GetOrCreateEncodeState();

        PlannedPacket planned{};
        if (!PollNextFromState(m_stream, state, ectx, planned))
        {
            return false;
        }

        /*
            Registry should copy PacketPlan::tick and PacketPlan::begin into
            EncodeContext before calling Poll.
        */
        if (planned.tick != ectx.packetTick)
        {
            return false;
        }

        if (planned.begin != ectx.begin)
        {
            return false;
        }

        packet = planned.packet;
        return true;
    }

    void AdvanceTick(const AdvanceTickContext& tctx) const
    {
        (void)tctx;

        /*
            Generic stream tick advancement.
        */
        m_stream.AdvanceTick();
    }

   private:
    EncodeState& GetOrCreateEncodeState() const
    {
        if (!m_hasEncodeState)
        {
            m_encodeState = EncodeState{};

            /*
                Start at current stream end so this net stream only transmits
                data produced after replication starts.
            */
            m_stream.AdvanceCursor(m_encodeState.cursor);

            m_encodeState.netCursor = NetCursor{0};
            m_hasEncodeState = true;
        }

        return m_encodeState;
    }

    static NetCursor AdvanceNetCursor(NetCursor cursor)
    {
        return NetCursor{
            static_cast<std::uint32_t>(++cursor)
        };
    }

    static bool PeekNextFromState(
        const Stream& stream,
        EncodeState& state,
        const EncodeContext& ectx,
        PlannedPacket& planned)
    {
        /*
            Peek uses a copied EncodeState, so the mutating implementation is
            safe here.
        */
        return PollNextFromState(stream, state, ectx, planned);
    }

    static bool PollNextFromState(
        const Stream& stream,
        EncodeState& state,
        const EncodeContext& ectx,
        PlannedPacket& planned)
    {
        planned = {};

        Frame frame{};

        if (!Adapter::ExtractFrame(stream, state.cursor, frame))
        {
            return false;
        }

        const NetCursor begin = state.netCursor;
        const NetCursor end = AdvanceNetCursor(begin);

        state.netCursor = end;

        /*
            Frame -> Packet conversion is assumed to be implicit.
        */
        Packet packet = frame;

        planned.tick = ectx.senderTick;
        planned.begin = begin;
        planned.end = end;
        planned.packet = packet;

        return true;
    }
};

template <typename TAdapter>
class LatestEventNetInputStream
{
   public:
    using Adapter = TAdapter;
    using Stream = typename Adapter::Stream;
    using Frame = typename Adapter::Frame;
    using Packet = typename Adapter::Packet;

   private:
    Stream& m_stream;

   public:
    explicit LatestEventNetInputStream(Stream& stream)
        : m_stream(stream)
    {
    }

    LatestEventNetInputStream(const LatestEventNetInputStream&) = delete;

    LatestEventNetInputStream& operator=(
        const LatestEventNetInputStream&) = delete;

    LatestEventNetInputStream(LatestEventNetInputStream&&) = delete;

    LatestEventNetInputStream& operator=(
        LatestEventNetInputStream&&) = delete;

    bool Insert(
        const DecodeContext& dctx,
        const Packet& packet)
    {
        (void)dctx;

        /*
            Packet -> Frame conversion is assumed to be implicit.
        */
        const Frame frame = packet;

        Adapter::InsertFrame(m_stream, frame);
        return true;
    }
};

template <typename TStream>
struct SimpleEventAdapter
{
    using Stream = TStream;
    using Frame = typename TStream::Event;
    using Packet = Frame;

    static bool ExtractFrame(
        const Stream& stream,
        typename Stream::Cursor& cursor,
        Frame& frame)
    {
        return stream.PollOne(cursor, frame);
    }

    static void InsertFrame(
        Stream& stream,
        const Frame& frame)
    {
        stream.Push(frame);
    }
};
}