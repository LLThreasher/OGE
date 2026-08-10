#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <unordered_map>
#include <vector>

#include "game/app_context.hpp"
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
    std::vector<std::byte> payload;
};

struct EventLogEntryRef
{
    EventLogEntryMeta entry;
    std::vector<std::byte>& payload;
};

struct EventLogEntryConstRef
{
    EventLogEntryMeta entry;
    std::vector<std::byte>& payload;
};

struct SendPayload
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
template <size_t Capacity = 256>
class EventLogStream
{
    static constexpr bool SHOW_LOGS = false;

   protected:
    oge::DiscreteEventStream<EventLogEntryMeta, Capacity> m_entries;
    std::bitset<Capacity> m_validSet = {};
    std::bitset<64> m_activePeers = {};
    std::unordered_map<LogCursor, std::vector<std::byte>> m_payloads;
    LogCursor m_currentTail = 1;
    AnythingFactory* m_af;
    oge_id_type m_sendPayloadTypeId = 0;

   public:
    EventLogStream(AnythingFactory* af = nullptr, bool authority = false)
        : m_af(af)
    {
        if (m_af) m_sendPayloadTypeId = m_af->Id<SendPayload>();
    }

    // this must be called at tick boundry
    void AddPeer(uint32_t peerId)
    {
        m_activePeers.set(peerId, true);
    }

    // this must be called at tick boundry
    void RemovePeer(uint32_t peerId)
    {
        m_activePeers.set(peerId, false);
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
                               const std::span<std::byte> payload)
    {
        uint32_t size = static_cast<uint32_t>(payload.size_bytes());
        buffer.Write(m_sendPayloadTypeId);
        buffer.Write(cursor);
        buffer.Write(size);
        buffer.WriteRaw(payload.data(), size);
    }

    void SerializeEvent(Buffer& buffer, const EventLogEntry& entry)
    {
        SerializeEventMeta(entry.entry, buffer);
        SerializeEventPayload(entry.entry.cursor, buffer, entry.payload);
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
                while (m_entries.HeadCursor() < claimedCursor)
                {
                    m_validSet.set(m_entries.HeadCursor() % Capacity, false);
                    EventLogEntryMeta empty = {};
                    empty.recieveMask = {};
                    m_entries.Push(empty);
                }
            }
            else
            {
                meta.cursor = m_entries.HeadCursor();
            }
            m_validSet.set(m_entries.HeadCursor() % Capacity, true);
            m_entries.Push(meta);

            if (SHOW_LOGS)
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
    std::vector<std::byte>* GetPayload(LogCursor cursor)
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
        auto [it, succ] = m_payloads.emplace(cursor, payloadSize);
        OGE_ASSERT(succ, "Failed to emplace payload at cursor {}", cursor);
        buffer.ReadRaw(it->second.data(), payloadSize);
    }

    Buffer EnqueueEvent(oge_id_type id, size_t initPayloadSize,
                        std::bitset<64> peerMask = ~std::bitset<64>{})
    {
        // if (m_af)
        //     LOG_DEBUG("enqueue event {} with mask {}",
        //     m_af->GetDescriptor(id)->name, (peerMask &
        //     m_activePeers).to_ullong());
        OGE_ASSERT(peerMask.any(), "EnqueueEvent called with empty peer mask");
        EventLogEntryMeta meta = {m_entries.HeadCursor(), id,
                                  peerMask & m_activePeers};
        m_validSet.set(m_entries.HeadCursor() % Capacity, true);
        m_entries.Push(meta);
        auto [it, succ] = m_payloads.try_emplace(meta.cursor);
        OGE_ASSERT(succ, "Failed to emplace payload at cursor {}", meta.cursor);
        it->second.resize(initPayloadSize);
        return {it->second};
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
        bool incrementTail = curosr == m_currentTail;
        while (m_entries.Contains(curosr))
        {
            if (m_validSet.test(curosr % Capacity))
            {
                incrementTail = false;
                EventLogEntryMeta& entry = m_entries.Get(curosr);
                if (entry.recieveMask.test(peer))
                {
                    if (SHOW_LOGS)
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
                        m_currentTail = oge::math::max(m_currentTail, curosr);
                        return true;
                    }
                }
                else if (entry.recieveMask.none())
                {
                    incrementTail = true;
                }
            }
            else if (incrementTail)
            {
                m_currentTail = oge::math::max(m_currentTail, curosr);
            }
            ++curosr;
        }
        return false;
    }

    void Update()
    {
        // increment tail to first valid entry
        while (m_currentTail < m_entries.HeadCursor() &&
               !m_validSet.test(m_currentTail % Capacity))
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
