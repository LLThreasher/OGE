#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <unordered_map>
#include <vector>

#include "game/app_context.hpp"
#include "oge/assert.hpp"
#include "oge/event_stream.hpp"
#include "oge/log.hpp"
#include "oge/math.hpp"
#include "oge/runtime/net_serializer.hpp"
#include "oge/runtime/type_name.hpp"

namespace game::net
{
using oge::runtime::oge_id_type;
using oge::runtime::net::Buffer;

using LogCursor = uint64_t;

// ---------------------------------------------------------------------------
// SmallPayload — small-buffer-optimized payload storage.
//
// Payloads ≤ kSmallPayloadInlineSize bytes are stored inline in `inlineBuf`
// (a public std::array users may write to directly).  Larger payloads fall
// back to a heap-allocated std::vector.
//
// Move and copy are both supported.  Copy is needed by PeekEvent (which
// copies from a const map into the caller's scratch buffer).  Move is used
// by TryDequeueEvent to hand ownership out of the stream.
// ---------------------------------------------------------------------------
constexpr size_t kSmallPayloadInlineSize = 64;

struct SmallPayload
{
    std::array<std::byte, kSmallPayloadInlineSize> inlineBuf{};

    std::byte* data()
    {
        return m_isInline ? inlineBuf.data() : m_heapBuf.data();
    }
    const std::byte* data() const
    {
        return m_isInline ? inlineBuf.data() : m_heapBuf.data();
    }

    size_t size() const
    {
        return m_size;
    }
    size_t capacity() const
    {
        return m_isInline ? kSmallPayloadInlineSize : m_heapBuf.capacity();
    }
    bool isInline() const
    {
        return m_isInline;
    }
    bool empty() const
    {
        return m_size == 0;
    }

    // Resize payload storage.  n ≤ kSmallPayloadInlineSize stays inline;
    // larger values promote to heap.
    void resize(size_t n)
    {
        if (n <= kSmallPayloadInlineSize)
        {
            if (!m_isInline)
            {
                // Demote from heap back to inline — copy existing data
                size_t keep = std::min(n, m_size);
                if (keep > 0)
                    std::memcpy(inlineBuf.data(), m_heapBuf.data(), keep);
                m_heapBuf.clear();
                m_heapBuf.shrink_to_fit();
                m_isInline = true;
            }
            m_size = n;
        }
        else
        {
            if (m_isInline)
            {
                // Promote to heap — move existing data
                m_heapBuf.assign(inlineBuf.data(), inlineBuf.data() + m_size);
                m_isInline = false;
            }
            m_heapBuf.resize(n);
            m_size = n;
        }
    }

    // For users who write directly into inlineBuf: commit the written size.
    void commitSize(size_t n)
    {
        OGE_ASSERT(m_isInline, "commitSize called on heap-backed SmallPayload");
        OGE_ASSERT(n <= kSmallPayloadInlineSize,
                   "committed size {} exceeds inline capacity", n);
        m_size = n;
    }

    // Access the internal heap buffer (only valid when !isInline()).
    // Provided for Buffer's owning-scratch constructor.
    std::vector<std::byte>& heapBuf()
    {
        OGE_ASSERT(!m_isInline, "heapBuf() called on inline SmallPayload");
        return m_heapBuf;
    }

    // -- default -------------------------------------------------------------

    SmallPayload() = default;

    // -- move ----------------------------------------------------------------

    SmallPayload(SmallPayload&& other) noexcept
        : inlineBuf(other.inlineBuf),
          m_heapBuf(std::move(other.m_heapBuf)),
          m_size(other.m_size),
          m_isInline(other.m_isInline)
    {
        other.m_size = 0;
        other.m_isInline = true;
    }

    SmallPayload& operator=(SmallPayload&& other) noexcept
    {
        if (this != &other)
        {
            if (m_isInline)
            {
                if (other.m_isInline)
                {
                    std::memcpy(inlineBuf.data(), other.inlineBuf.data(),
                                other.m_size);
                }
                else
                {
                    m_heapBuf = std::move(other.m_heapBuf);
                    m_isInline = false;
                }
            }
            else
            {
                if (other.m_isInline)
                {
                    m_heapBuf.clear();
                    m_heapBuf.shrink_to_fit();
                    std::memcpy(inlineBuf.data(), other.inlineBuf.data(),
                                other.m_size);
                    m_isInline = true;
                }
                else
                {
                    m_heapBuf = std::move(other.m_heapBuf);
                }
            }
            m_size = other.m_size;
            other.m_size = 0;
            other.m_isInline = true;
        }
        return *this;
    }

    // -- copy ----------------------------------------------------------------

    SmallPayload(const SmallPayload& other)
        : m_size(other.m_size), m_isInline(other.m_isInline)
    {
        if (m_isInline)
        {
            std::memcpy(inlineBuf.data(), other.inlineBuf.data(), m_size);
        }
        else
        {
            m_heapBuf = other.m_heapBuf;
        }
    }

    SmallPayload& operator=(const SmallPayload& other)
    {
        if (this != &other)
        {
            m_size = other.m_size;
            if (other.m_isInline)
            {
                if (!m_isInline)
                {
                    m_heapBuf.clear();
                    m_heapBuf.shrink_to_fit();
                    m_isInline = true;
                }
                std::memcpy(inlineBuf.data(), other.inlineBuf.data(), m_size);
            }
            else
            {
                if (m_isInline)
                {
                    m_isInline = false;
                }
                m_heapBuf = other.m_heapBuf;
            }
        }
        return *this;
    }

    // -- comparison ----------------------------------------------------------

    bool operator==(const SmallPayload& other) const
    {
        if (m_size != other.m_size) return false;
        return std::memcmp(data(), other.data(), m_size) == 0;
    }
    bool operator!=(const SmallPayload& other) const
    {
        return !(*this == other);
    }

    // Compare against any contiguous byte range (std::span, std::vector, etc.)
    bool operator==(std::span<const std::byte> other) const
    {
        if (m_size != other.size()) return false;
        return std::memcmp(data(), other.data(), m_size) == 0;
    }
    bool operator!=(std::span<const std::byte> other) const
    {
        return !(*this == other);
    }

    // -- implicit conversion to span -----------------------------------------

    operator std::span<std::byte>()
    {
        return {data(), m_size};
    }
    operator std::span<const std::byte>() const
    {
        return {data(), m_size};
    }

    // -- explicit span access ------------------------------------------------

    std::span<std::byte> span()
    {
        return {data(), m_size};
    }
    std::span<const std::byte> span() const
    {
        return {data(), m_size};
    }

   private:
    std::vector<std::byte> m_heapBuf{};
    size_t m_size = 0;
    bool m_isInline = true;
};

// 24 bytes
struct EventLogEntryMeta
{
    LogCursor cursor = 0;
    oge_id_type id = 0;
    // last bit is used to indicate whether payload exists
    std::bitset<64> recieveMask = {};
};

struct EventLogEntry
{
    EventLogEntryMeta entry;
    SmallPayload payload;
};

struct EventLogEntryRef
{
    EventLogEntryMeta entry;
    SmallPayload& payload;
};

struct EventLogEntryConstRef
{
    EventLogEntryMeta entry;
    SmallPayload& payload;
};

struct SendPayload
{
};

// when a peer joins, we queue an add peer event to the new peer
// since server won't send events to a peer untill the ready
// packet arrives, this will block the tail untill peer is ready
// it will also give the peer info on the tail location
struct UpdateTail
{
};

constexpr uint32_t MyPeerId = 63;

// the event stream protocol follows a roughly consistent principle
//   log cursor starts at 1.
//   each peer writes its own head cursor into packets.
//   the receiver pads its log with empty entries up to a claim that lies
//      ahead of its head, so both sides number the event identically;
//      a claim behind the receiver's head is stale (the sender has not
//      seen the receiver's newer events) and is appended at the local
//      head with the local cursor — local events may not keep the same
//      cursor slot as on the sender's log.
//   the authority peer must maintain a full event log, including events
//      every non-authoritative peer.
//   non-authoritative peer only maintain local events and remote events
//      with its mask. local events may not have the same cursor slot as
//      its replica on the authoritative log.
template <size_t Capacity = 32768>
class EventLogStream
{
    static constexpr bool SHOW_LOGS = false;

   protected:
    oge::DiscreteEventStream<EventLogEntryMeta, Capacity> m_entries;
    std::bitset<Capacity> m_validSet = {};
    std::bitset<64> m_activePeers = {};
    std::unordered_map<LogCursor, SmallPayload> m_payloads;
    LogCursor m_currentTail = 1;
    AnythingFactory* m_af;
    oge_id_type m_sendPayloadTypeId = 0;
    oge_id_type m_addPeerTypeId = 1;

   public:
    EventLogStream(AnythingFactory* af = nullptr, bool authority = false)
        : m_af(af)
    {
        if (m_af)
        {
            m_sendPayloadTypeId = m_af->Id<SendPayload>();
            m_addPeerTypeId = m_af->Id<UpdateTail>();
        }
    }

    // this must be called at tick boundry
    void AddPeer(uint32_t peerId)
    {
        m_activePeers.set(peerId, true);
        // auto buf = EnqueueEvent(m_addPeerTypeId, sizeof(LogCursor));
        // buf.template Write<LogCursor>(m_currentTail);
    }

    // this must be called at tick boundry
    void RemovePeer(uint32_t peerId)
    {
        m_activePeers.set(peerId, false);
        LogCursor cursor = m_currentTail;
        while (cursor < m_entries.HeadCursor())
        {
            if (m_validSet.test(cursor % Capacity) &&
                (m_entries.Get(cursor).recieveMask & m_activePeers).none())
            {
                m_validSet.set(cursor % Capacity, false);
            }
            ++cursor;
        }
    }

    void SerializeEventMeta(Buffer& buffer, const EventLogEntryMeta& meta)
    {
        if (SHOW_LOGS)
            LOG_DEBUG("serialize event {} at slot {}",
                      m_af->GetDescriptor(meta.id)->name, meta.cursor);

        buffer.Write(meta.id);
        buffer.Write<LogCursor>(meta.cursor);
    }

    // SerializeEventPayload prefixes the payload with the send-payload
    // type id, the cursor, and its byte size.  Packet allocations must
    // reserve these extra bytes (see ProduceAll).
    static constexpr size_t PayloadSizePrefixBytes()
    {
        return sizeof(uint32_t);
    }

    // Total header bytes written by SerializeEventPayload before the raw
    // payload: [sendPayloadTypeId][cursor][uint32 size].
    static constexpr size_t PayloadHeaderBytes()
    {
        return sizeof(oge_id_type) + sizeof(LogCursor) + sizeof(uint32_t);
    }

    void SerializeEventPayload(LogCursor cursor, Buffer& buffer,
                               std::span<const std::byte> payload)
    {
        uint32_t size = static_cast<uint32_t>(payload.size());
        buffer.Write(m_sendPayloadTypeId);
        buffer.Write(cursor);
        buffer.Write(size);
        buffer.WriteRaw(payload.data(), size);
    }

    void SerializeEvent(Buffer& buffer, const EventLogEntry& entry)
    {
        SerializeEventMeta(entry.entry, buffer);
        SerializeEventPayload(entry.entry.cursor, buffer,
                              std::span<const std::byte>(entry.payload.data(),
                                                         entry.payload.size()));
    }

    static constexpr size_t MetaSize()
    {
        return sizeof(LogCursor) + sizeof(oge_id_type);
    }

    EventLogEntryMeta DeserializeEvent(uint32_t peerId, Buffer& buffer)
    {
        EventLogEntryMeta meta{};
        buffer.Read(meta.id);
        buffer.Read(meta.cursor);
        meta.recieveMask = {};

        if (meta.id == m_sendPayloadTypeId)
        {
            // Payload-only packet (StreamReliable channel 1).  The meta read
            // above consumed the [sendPayloadTypeId][cursor] prefix, so only
            // [size][payload] remains.
            DeserializeEventPayload(meta.cursor, buffer);
        }
        else if (meta.id == m_addPeerTypeId)
        {
            LogCursor tail;
            buffer.Read(tail);
            m_currentTail = tail;

            EventLogEntryMeta empty = {};
            empty.recieveMask = {};
            while (m_entries.HeadCursor() < m_currentTail)
            {
                m_entries.Push(empty);
            }
        }
        else
        {
            // The claimed cursor is the sender's number for this event.  If
            // the local log has not reached it yet, pad with empty entries
            // so both sides number the event identically.  If the local log
            // has already passed it (the sender is behind — e.g. a client
            // sending input while the server generated events the client
            // has not seen yet), the claim is stale: append at the local
            // head with the local cursor instead.  Keying the payload at
            // the stale claim would collide with the local event still
            // occupying that slot.
            const LogCursor claimedCursor = meta.cursor;
            if (m_entries.HeadCursor() < claimedCursor)
            {
                EventLogEntryMeta empty = {};
                empty.recieveMask = {};
                while (m_entries.HeadCursor() < claimedCursor)
                {
                    m_validSet.set(m_entries.HeadCursor() % Capacity, false);
                    m_entries.Push(empty);
                }
            }
            else
            {
                meta.cursor = m_entries.HeadCursor();
            }
            m_validSet.set(m_entries.HeadCursor() % Capacity, true);
            m_entries.Push(meta);

            if (SHOW_LOGS && m_af->GetDescriptor(meta.id))
                LOG_DEBUG("deserialize event {} at slot {} ({})",
                          m_af->GetDescriptor(meta.id)->name, meta.cursor,
                          meta.cursor % Capacity);

            if (!buffer.IsEmpty())
            {
                // Single packet: the payload section starts with its own
                // [sendPayloadTypeId][cursor] prefix.  The prefix carries
                // the sender's claimed cursor, which may differ from the
                // slot assigned above.
                ConsumePayloadHeader(claimedCursor, buffer);
                DeserializeEventPayload(meta.cursor, buffer);
            }
        }

        return meta;
    }

    // True if `id` is the stream's send-payload type id — i.e. the packet
    // carried only the payload half of a StreamReliable event, without its
    // meta half.
    bool IsSendPayloadType(oge_id_type id) const
    {
        return id == m_sendPayloadTypeId;
    }

    // Returns the meta stored at `cursor`, or nullptr if no entry exists
    // there yet (the meta half of a split packet has not arrived, or the
    // slot is a gap pad).
    const EventLogEntryMeta* GetEntry(LogCursor cursor) const
    {
        if (!m_entries.Contains(cursor) || !m_validSet.test(cursor % Capacity))
        {
            return nullptr;
        }
        return &m_entries.Get(cursor);
    }

    // Returns the payload stored at `cursor`, or nullptr if it has not
    // arrived yet (the payload half of a StreamReliable event is still in
    // flight).
    SmallPayload* GetPayload(LogCursor cursor)
    {
        auto it = m_payloads.find(cursor);
        return it == m_payloads.end() ? nullptr : &it->second;
    }

    // Consumes and validates the payload section header
    // [sendPayloadTypeId][cursor] written by SerializeEventPayload.
    void ConsumePayloadHeader(LogCursor cursor, Buffer& buffer)
    {
        oge_id_type payloadTypeId;
        buffer.Read(payloadTypeId);
        LogCursor payloadCursor;
        buffer.Read(payloadCursor);
        OGE_ASSERT(payloadTypeId == m_sendPayloadTypeId,
                   "Payload section does not start with the send-payload id "
                   "({} != {})",
                   payloadTypeId, m_sendPayloadTypeId);
        OGE_ASSERT(payloadCursor == cursor,
                   "Payload section cursor {} does not match event cursor {}",
                   payloadCursor, cursor);
    }

    void DeserializeEventPayload(LogCursor cursor, Buffer& buffer)
    {
        uint32_t payloadSize;
        buffer.Read(payloadSize);
        auto [it, succ] = m_payloads.try_emplace(cursor);
        OGE_ASSERT(succ, "Failed to emplace payload at cursor {}", cursor);
        it->second.resize(payloadSize);
        buffer.ReadRaw(it->second.data(), payloadSize);
    }

    Buffer EnqueueEvent(oge_id_type id, size_t initPayloadSize,
                        std::bitset<64> peerMask = ~std::bitset<64>{})
    {
        if (SHOW_LOGS)
            LOG_DEBUG("enqueue event {} with mask {} at {} with tail {}",
                      m_af->GetDescriptor(id)->name,
                      (peerMask & m_activePeers).to_ullong(),
                      m_entries.HeadCursor(), m_currentTail);

        OGE_ASSERT(m_entries.HeadCursor() - m_currentTail < Capacity,
                   "too many enqueue, tail wrap around at {}, with head {}",
                   m_currentTail, m_entries.HeadCursor());
        OGE_ASSERT(peerMask.any(), "EnqueueEvent called with empty peer mask");
        EventLogEntryMeta meta = {m_entries.HeadCursor(), id,
                                  peerMask & m_activePeers};
        m_validSet.set(m_entries.HeadCursor() % Capacity, true);
        m_entries.Push(meta);
        auto [it, succ] = m_payloads.try_emplace(meta.cursor);
        OGE_ASSERT(succ, "Failed to emplace payload at cursor {}", meta.cursor);
        it->second.resize(initPayloadSize);
        if (it->second.isInline())
        {
            // Non-owning buffer over the inline array — cannot grow, but
            // initPayloadSize is the exact serialized size so overflow
            // never occurs.
            return Buffer(it->second.data(), it->second.size());
        }
        else
        {
            // Owning-scratch buffer over the heap vector — may grow via
            // EnsureCapacity if needed.
            return Buffer(it->second.heapBuf());
        }
    }

    bool PeekEvent(uint32_t peer, EventLogEntryConstRef& out,
                   LogCursor at = 0) const
    {
        auto curosr = at == 0 ? m_currentTail : at;
        if (SHOW_LOGS)
            LOG_DEBUG("peek event at slot {}: {}, {}, {}", curosr,
                      m_currentTail, m_entries.Contains(curosr),
                      m_validSet.test(curosr % Capacity));
        while (m_entries.Contains(curosr))
        {
            if (m_validSet.test(curosr % Capacity))
            {
                const EventLogEntryMeta& entry = m_entries.Get(curosr);
                if (entry.recieveMask.test(peer))
                {
                    out.entry = entry;
                    auto it = m_payloads.find(curosr);
                    if (it != m_payloads.end())
                    {
                        out.payload = std::move(it->second);
                        return true;
                    }
                }
            }
            ++curosr;
        }
        return false;
    }

    bool TryDequeueEvent(uint32_t peer, EventLogEntry& out, LogCursor at = 0)
    {
        auto curosr = at == 0 ? m_currentTail : at;
        while (m_entries.Contains(curosr))
        {
            if (m_validSet.test(curosr % Capacity))
            {
                EventLogEntryMeta& entry = m_entries.Get(curosr);
                if (entry.recieveMask.test(peer))
                {
                    if (SHOW_LOGS && m_af->GetDescriptor(entry.id))
                        LOG_DEBUG("deqeue event {} at slot {}",
                                  m_af->GetDescriptor(entry.id)->name,
                                  entry.cursor);
                    out.entry = entry;
                    auto it = m_payloads.find(curosr);
                    if (it != m_payloads.end())
                    {
                        entry.recieveMask.set(peer, false);
                        if (entry.recieveMask.none())
                            m_validSet.set(curosr % Capacity, false);
                        out.payload = std::move(it->second);
                        m_payloads.erase(it);
                        return true;
                    }
                }
                else if ((entry.recieveMask & m_activePeers).none())
                {
                    m_validSet.set(curosr % Capacity, false);
                }
            }
            ++curosr;
        }
        return false;
    }

    void Update()
    {
        // increment tail to first valid entry
        while (m_currentTail < m_entries.HeadCursor() &&
               (!m_validSet.test(m_currentTail % Capacity)))
        {
            m_currentTail++;
        }

        // we skip tick 0 intentionally, because we use tick 0 to load
        //      world save.
        if (m_entries.HeadCursor() <= Capacity) return;

        auto nextTail = m_entries.HeadCursor() - Capacity;
        while (m_currentTail < nextTail)
        {
            if (m_payloads.contains(m_currentTail))
            {
                m_payloads.erase(m_currentTail);
            }

            // OGE_ASSERT(!m_validSet.test(m_currentTail % Capacity),
            //            "unhandled event dies at boundary {}", m_currentTail);
            m_validSet.set(m_currentTail % Capacity, false);

            ++m_currentTail;
        }
    }
};
}  // namespace game::net
