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
#include "oge/runtime/debug.hpp"
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
    uint32_t senderId = 0;
    // last bit is used to indicate whether payload exists
    std::bitset<64> recieveMask = ~std::bitset<64>{};
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

constexpr uint32_t MyPeerId = 63;

// the event stream protocol follows a roughly consistent principle
//   log cursor starts at 0. 
//   only one authority peer is allowed. 
//   authority peer writes the log cursor into packet,
//   non-authority peer writes 0 for log cursor. 
//   authority peer must maintain a full event log, including events
//      every non-authoritative peer. 
//   non-authoritative peer only maintain local events and remote events
//      with its mask. local events may not have the same cursor slot as
//      its replica on the authoritative log. 
template <size_t Capacity = 4096>
class EventLogStream
{
   protected:
    oge::DiscreteEventStream<EventLogEntryMeta, Capacity> m_entries;
    std::bitset<Capacity> m_validSet = {};
    std::bitset<64> m_activePeers = {};
    std::unordered_map<LogCursor, std::vector<std::byte>> m_payloads;
    LogCursor m_currentTail = 1;
    AnythingFactory* m_af;

   public:
    EventLogStream(AnythingFactory* af = nullptr, bool authority = false) : m_af(af)
    {
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
        LOG_DEBUG("serialize event {} at slot {}", m_af->GetDescriptor(meta.id)->name, meta.cursor);
        
        buffer.Write<LogCursor>(meta.cursor);
        buffer.Write(meta.id);
    }

    // SerializeEventPayload prefixes the payload with its byte size (uint32_t).
    // Packet allocations must reserve these extra bytes (see ProduceAll).
    static constexpr size_t PayloadSizePrefixBytes()
    {
        return sizeof(uint32_t);
    }

    void SerializeEventPayload(Buffer& buffer,
                               const std::span<std::byte> payload)
    {
        uint32_t size = static_cast<uint32_t>(payload.size_bytes());
        buffer.Write(size);
        buffer.WriteRaw(payload.data(), size);
    }

    void SerializeEvent(Buffer& buffer, const EventLogEntry& entry)
    {
        SerializeEventMeta(entry.entry, buffer);
        SerializeEventPayload(entry.payload, buffer);
    }

    static constexpr size_t MetaSize()
    {
        return sizeof(LogCursor) + sizeof(oge_id_type);
    }

    void DeserializeEvent(uint32_t peerId, Buffer& buffer)
    {
        EventLogEntryMeta meta{};
        buffer.Read(meta.cursor);
        buffer.Read(meta.id);
        meta.recieveMask = {};

        // if the local head is smaller, increment untill the proposed slot
        while (m_entries.HeadCursor() < meta.cursor)
        {
            m_validSet.set(m_entries.HeadCursor() % Capacity, false);
            m_entries.Push({});
        }
        // if the remote head is smaller, insert directly into loca head slot
        m_validSet.set(m_entries.HeadCursor() % Capacity, true);
        m_entries.Push(meta);

        LOG_DEBUG("deserialize event {} at slot {}", m_af->GetDescriptor(meta.id)->name, meta.cursor);

        if (!buffer.IsEmpty())
        {
            DeserializeEventPayload(meta.cursor, buffer);
        }
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
        //     LOG_DEBUG("enqueue event {} with mask {}", m_af->GetDescriptor(id)->name, (peerMask & m_activePeers).to_ullong());
        OGE_ASSERT(peerMask.any(), "EnqueueEvent called with empty peer mask");
        EventLogEntryMeta meta = {m_entries.HeadCursor(), id, 0,
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
        bool incrementTail = true;
        while (m_entries.Contains(curosr))
        {
            if (m_validSet.test(curosr % Capacity))
            {
                const EventLogEntryMeta& entry = m_entries.Get(curosr);
                if (entry.recieveMask.test(peer))
                {
                    auto it = m_payloads.find(curosr);
                    if (it != m_payloads.end())
                    {
                        out.entry = entry;
                        out.payload = it->second;
                        return true;
                    }
                    else
                    {
                        incrementTail = false;
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
        bool incrementTail = true;
        while (m_entries.Contains(curosr))
        {
            if (m_validSet.test(curosr % Capacity))
            {
                EventLogEntryMeta& entry = m_entries.Get(curosr);
                if (entry.recieveMask.test(peer))
                {
                    out.entry = entry;
                    auto it = m_payloads.find(curosr);
                    if (it != m_payloads.end())
                    {
                        entry.recieveMask.set(peer, false);
                        if (entry.recieveMask.none())
                            m_validSet.set(curosr, false);
                        out.payload = std::move(it->second);
                        m_payloads.erase(it);
                        return true;
                    }
                    else
                    {
                        incrementTail = false;
                    }
                }
            }
            if (incrementTail) ++m_currentTail;
            ++curosr;
        }
        return false;
    }

    void Update()
    {
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

            OGE_ASSERT(!m_validSet.test(m_currentTail),
                       "unhandled event dies at boundary");

            ++m_currentTail;
        }
    }
};
}  // namespace game::net
