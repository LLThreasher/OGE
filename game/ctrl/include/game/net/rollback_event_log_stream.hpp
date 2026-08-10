#pragma once

#include <deque>
#include <unordered_map>
#include <vector>

#include "game/net/event_log_stream.hpp"
#include "game/net/replication_registry.hpp"
#include "game/net/rollback_capability.hpp"
#include "oge/runtime/typed_registry.hpp"

namespace game::net
{

// =========================================================================
// RollbackEventLogStream
//
// Extends EventLogStream with client-side prediction and rollback support.
//
// Lifecycle
// ---------
// 1. Server events arrive via the normal HandleIncoming path → stored in
//    the base EventLogStream.
// 2. The client may *predict* events locally via InsertPredicted().  Only
//    event types with a registered RollbackCapability are accepted.
// 3. Every snapshotInterval ticks, AdvanceTick() snapshots the world state
//    using registered RollbackCapability::takeSnapshot functions.
// 4. Validate() compares predicted events against server events.  If they
//    diverge, the latest snapshot is restored via rollback, then server
//    events are re-applied.
//
// Predicted events live in a separate buffer and do NOT enter the base
// EventLogStream (which only holds authoritative server events).
// =========================================================================

template <size_t Capacity = 32768>
class RollbackEventLogStream : public EventLogStream<Capacity>
{
   public:
    using Base = EventLogStream<Capacity>;

    // Forward to base constructor so the stream can receive AnythingFactory*
    // and authority flag (same signature as EventLogStream).
    RollbackEventLogStream(AnythingFactory* af = nullptr,
                           bool authority = false)
        : Base(af, authority)
    {
    }

    // ---------- Snapshot management ----------

    struct SnapshotPoint
    {
        LogCursor streamCursor = 0;
        std::unordered_map<FamilyId, std::pmr::vector<std::byte>> payloads;
    };

    // ---------- Predicted events ----------

    struct PredictedEntry
    {
        FamilyId family = 0;
        std::vector<std::byte> payload;
    };

    // ---------- Configuration ----------

    Tick m_snapshotInterval = 20;
    size_t m_maxSnapshots = 32;

    // ---------- Registration ----------

    void RegisterRollbackCapability(const RollbackCapability& cap)
    {
        m_rollbackCaps[cap.family] = cap;
    }

    bool HasCapability(FamilyId family) const
    {
        return m_rollbackCaps.contains(family);
    }

    // ---------- Tick ----------

    Tick CurrentTick() const { return m_currentTick; }

    void AdvanceTick(oge::runtime::OgeRegistryRef world)
    {
        ++m_currentTick;

        if (m_currentTick % m_snapshotInterval == 0)
        {
            TakeSnapshot(world);
        }

        while (m_snapshots.size() > m_maxSnapshots)
            m_snapshots.pop_front();

        // Keep only recent predictions (since last snapshot).
        if (!m_snapshots.empty())
        {
            auto cutoff = m_snapshots.front().streamCursor;
            while (!m_predictedEvents.empty() &&
                   m_predictedEvents.front().family == 0)
                m_predictedEvents.pop_front();
        }
    }

    // ---------- Prediction ----------

    bool InsertPredicted(FamilyId family,
                         const std::vector<std::byte>& payload)
    {
        if (!m_rollbackCaps.contains(family)) return false;
        m_predictedEvents.push_back({family, payload});
        return true;
    }

    template <typename TEvent>
    bool InsertPredicted(const TEvent& event)
    {
        FamilyId family = entt::type_hash<TEvent>::value();
        if (!m_rollbackCaps.contains(family)) return false;

        size_t sz = net::Size(event);
        std::vector<std::byte> scratch;
        scratch.reserve(sz);
        scratch.resize(scratch.capacity());
        net::Buffer buf(scratch);
        net::Serialize(buf, event);

        PredictedEntry entry{};
        entry.family = family;
        entry.payload.assign(buf.Data().data(),
                             buf.Data().data() + buf.Size());
        m_predictedEvents.push_back(std::move(entry));
        return true;
    }

    // ---------- Validation ----------

    // Compare predicted events against server events.  If they diverge,
    // roll back to the latest snapshot.  Returns true if consistent.
    bool Validate(oge::runtime::OgeRegistryRef world)
    {
        if (m_snapshots.empty()) return true;

        // Build per-family lists of predicted payloads.
        std::unordered_map<FamilyId, std::vector<net::Buffer>> predByFam;
        for (auto& pe : m_predictedEvents)
        {
            net::Buffer b(const_cast<std::byte*>(pe.payload.data()),
                          pe.payload.size());
            b.ToReadOnly();
            predByFam[pe.family].push_back(b);
        }

        // Build per-family lists of server payloads since last snapshot.
        std::unordered_map<FamilyId, std::vector<net::Buffer>> servByFam;
        LogCursor cursor = m_snapshots.back().streamCursor;
        EventLogEntryConstRef ref{{}, m_scratchPayload};
        while (this->PeekEvent(0, ref, cursor))
        {
            net::Buffer b(ref.payload.data(), ref.payload.size());
            b.ToReadOnly();
            servByFam[ref.entry.id].push_back(b);
            cursor = ref.entry.cursor + 1;
        }

        // Compare per-family.
        bool consistent = true;
        for (auto& [family, cap] : m_rollbackCaps)
        {
            if (!cap.compare) continue;
            auto& pred = predByFam[family];
            auto& serv = servByFam[family];
            if (pred.size() != serv.size()) { consistent = false; break; }
            for (size_t i = 0; i < pred.size(); ++i)
            {
                if (!cap.compare(pred[i], serv[i]))
                {
                    consistent = false;
                    break;
                }
            }
            if (!consistent) break;
        }

        if (!consistent) RollbackToLatest(world);

        // Clear predictions after validation.
        m_predictedEvents.clear();

        return consistent;
    }

    // Compare only the most recent predicted payload against the most
    // recent server payload per family (since the last snapshot).
    // Intended for position prediction where per-frame rates differ
    // between client and server (exact count match is impossible).
    // On mismatch: RollbackToLatest + clear predictions.
    // Returns true if consistent.
    bool ValidateLatest(oge::runtime::OgeRegistryRef world)
    {
        if (m_snapshots.empty()) return true;

        // Find last predicted payload per family.
        std::unordered_map<FamilyId, const PredictedEntry*> lastPred;
        for (auto& pe : m_predictedEvents)
            lastPred[pe.family] = &pe;

        // Find last server payload per family since last snapshot.
        std::unordered_map<FamilyId, SmallPayload> lastServer;
        EventLogEntryConstRef ref{{}, m_scratchPayload};
        LogCursor cursor = m_snapshots.back().streamCursor;
        while (this->PeekEvent(0, ref, cursor))
        {
            lastServer[ref.entry.id] = ref.payload;
            cursor = ref.entry.cursor + 1;
        }

        // Compare per-family: last predicted vs last server.
        bool consistent = true;
        for (auto& [family, cap] : m_rollbackCaps)
        {
            if (!cap.compare) continue;
            auto p = lastPred.find(family);
            auto s = lastServer.find(family);
            if (p == lastPred.end() || s == lastServer.end())
                continue;  // no data yet for this family

            net::Buffer pb(
                const_cast<std::byte*>(p->second->payload.data()),
                p->second->payload.size());
            pb.ToReadOnly();
            net::Buffer sb(s->second.data(), s->second.size());
            sb.ToReadOnly();
            if (!cap.compare(pb, sb))
            {
                consistent = false;
                break;
            }
        }

        if (!consistent) RollbackToLatest(world);
        m_predictedEvents.clear();
        return consistent;
    }

    // ---------- Rollback ----------

    void RollbackToLatest(oge::runtime::OgeRegistryRef world)
    {
        if (m_snapshots.empty()) return;

        const auto& snap = m_snapshots.back();
        for (auto& [family, payload] : snap.payloads)
        {
            auto it = m_rollbackCaps.find(family);
            if (it == m_rollbackCaps.end() || !it->second.rollback)
                continue;

            net::Buffer buf(const_cast<std::byte*>(payload.data()),
                            payload.size());
            buf.ToReadOnly();
            it->second.rollback(world, buf);
        }

        m_predictedEvents.clear();
    }

    // ---------- Accessors ----------

    const auto& Snapshots() const { return m_snapshots; }
    const auto& PredictedEvents() const { return m_predictedEvents; }
    size_t PredictedCount() const { return m_predictedEvents.size(); }

    LogCursor LastSnapshotCursor() const
    {
        return m_snapshots.empty() ? 0 : m_snapshots.back().streamCursor;
    }

   private:
    void TakeSnapshot(oge::runtime::OgeRegistryRef world)
    {
        SnapshotPoint snap{};
        snap.streamCursor = this->m_currentTail;
        for (auto& [family, cap] : m_rollbackCaps)
        {
            if (cap.takeSnapshot)
                snap.payloads[family] = cap.takeSnapshot(world);
        }
        m_snapshots.push_back(std::move(snap));
    }

    Tick m_currentTick = 0;
    std::unordered_map<FamilyId, RollbackCapability> m_rollbackCaps;
    std::deque<SnapshotPoint> m_snapshots;
    std::deque<PredictedEntry> m_predictedEvents;
    mutable SmallPayload m_scratchPayload;
};

}  // namespace game::net
