#pragma once

#include <algorithm>
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
        // Client tick this snapshot was taken on.
        Tick tick = 0;
        LogCursor streamCursor = 0;
        std::unordered_map<FamilyId, std::pmr::vector<std::byte>> payloads;
    };

    // ---------- Predicted events ----------

    struct PredictedEntry
    {
        // Client tick the prediction was made for.
        Tick tick = 0;
        LogCursor streamCursor = 0;
        FamilyId family = 0;
        std::vector<std::byte> payload;
    };

    // Index range into m_predictedEvents for the predictions made during
    // one client tick.  The stream cursor cannot distinguish ticks (all
    // predictions of one frame share the tail cursor), so validation
    // offsets prediction windows by these ranges.
    struct TickPredictionRange
    {
        Tick tick = 0;
        size_t startIdx = 0;
        size_t count = 0;
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

    void AdvanceLocalTick(oge::runtime::OgeRegistryRef world)
    {
        ++m_currentTick;

        if (m_currentTick % m_snapshotInterval == 0)
        {
            TakeSnapshot(world);
        }

        while (m_snapshots.size() > m_maxSnapshots)
            m_snapshots.pop_front();

        // Keep only recent predictions (since the oldest snapshot).  Prune
        // by tick: the stream cursor recorded at insertion cannot tell
        // ticks apart (all predictions of one frame share the tail cursor).
        if (!m_snapshots.empty())
        {
            auto cutoffTick = m_snapshots.front().tick;
            while (!m_predictedEvents.empty() &&
                   m_predictedEvents.front().tick < cutoffTick)
                m_predictedEvents.pop_front();
            RebuildTickRanges();
        }
    }

    Tick CurrentTick() const { return m_currentTick; }

    // client -> server : server writes peer tick, which is the last tick the server has processed for this client
    // if simply sync client tick with server, the client will always be behind the server, and the server will always be ahead of the client, which is not good for prediction
    // so we need to keep track of the server tick and the peer cursor, which is the last cursor the server has processed for this client
    void AdvanceTick(oge::runtime::OgeRegistryRef world, AdvanceTick avt)
    {
        m_serverCursor = avt.peerCursor;
        m_serverTick = avt.tick;

        if (m_currentTick < m_serverTick)
        {
            m_currentTick = m_serverTick + m_snapshotInterval;  // advance to next snapshot interval
            LOG_WARN("client tick behind server tick, advancing to catch up");
            m_snapshots.clear();
            m_predictedEvents.clear();
            m_tickPredictionRanges.clear();
        }
    }

    // ---------- Prediction ----------

    bool InsertPredicted(FamilyId family,
                         const std::vector<std::byte>& payload)
    {
        if (!m_rollbackCaps.contains(family)) return false;
        m_predictedEvents.push_back(
            {m_currentTick, this->m_currentTail, family, payload});
        AppendTickRange();
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
        entry.tick = m_currentTick;
        entry.streamCursor = this->m_currentTail;
        entry.family = family;
        entry.payload.assign(buf.Data().data(),
                             buf.Data().data() + buf.Size());
        m_predictedEvents.push_back(std::move(entry));
        AppendTickRange();
        return true;
    }

    // ---------- Validation ----------

    // Compare predicted events against server events.  If they diverge,
    // roll back to the latest snapshot.  Returns true if consistent.
    bool Validate(oge::runtime::OgeRegistryRef world)
    {
        // Suspended while a pong is in flight: the snapshot history is
        // being realigned and comparing against unaligned snapshots could
        // roll back to the wrong state.
        if (m_waitingPong) return true;

        if (m_snapshots.empty()) return true;

        const SnapshotPoint* base = SelectBaseSnapshot();
        Tick baseTick = base->tick;

        // Build per-family lists of predicted payloads, only for
        // predictions made at-or-after the base snapshot (older
        // predictions are subsumed in the snapshot state).
        std::unordered_map<FamilyId, std::vector<net::Buffer>> predByFam;
        for (auto& range : m_tickPredictionRanges)
        {
            if (range.tick < baseTick) continue;
            for (size_t i = range.startIdx;
                 i < range.startIdx + range.count &&
                 i < m_predictedEvents.size();
                 ++i)
            {
                auto& pe = m_predictedEvents[i];
                net::Buffer b(const_cast<std::byte*>(pe.payload.data()),
                              pe.payload.size());
                b.ToReadOnly();
                predByFam[pe.family].push_back(b);
            }
        }

        // Build per-family lists of server payloads since the base
        // snapshot.  Incoming server events carry the self bit (peer 63);
        // TryDequeueEvent consumes each exactly once (clears the bit,
        // invalidates the entry and erases its payload), so the scan only
        // picks up events not yet consumed by an earlier validation — the
        // tail has advanced past everything below CurrentTail().
        std::unordered_map<FamilyId, std::vector<SmallPayload>> servByFam;
        EventLogEntry entry;
        LogCursor cursor = std::max(base->streamCursor, this->CurrentTail());
        while (this->TryDequeueEvent(63, entry, cursor))
        {
            servByFam[entry.entry.id].push_back(entry.payload);
            cursor = entry.entry.cursor + 1;
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
                net::Buffer sb(serv[i].data(), serv[i].size());
                sb.ToReadOnly();
                if (!cap.compare(pred[i], sb))
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
        m_tickPredictionRanges.clear();

        return consistent;
    }

    // Compare only the most recent predicted payload against the most
    // recent server payload per family (since the base snapshot).
    // Intended for position prediction where per-frame rates differ
    // between client and server (exact count match is impossible).
    // On mismatch: RollbackToLatest + clear predictions.
    // Returns true if consistent.
    bool ValidateLatest(oge::runtime::OgeRegistryRef world)
    {
        // Suspended while a pong is in flight — see Validate().
        if (m_waitingPong) return true;

        if (m_snapshots.empty()) return true;

        const SnapshotPoint* base = SelectBaseSnapshot();
        Tick baseTick = base->tick;

        // Find last predicted payload per family, only for predictions
        // made at-or-after the base snapshot.
        std::unordered_map<FamilyId, const PredictedEntry*> lastPred;
        for (auto& range : m_tickPredictionRanges)
        {
            if (range.tick < baseTick) continue;
            for (size_t i = range.startIdx;
                 i < range.startIdx + range.count &&
                 i < m_predictedEvents.size();
                 ++i)
            {
                lastPred[m_predictedEvents[i].family] =
                    &m_predictedEvents[i];
            }
        }

        // Find last server payload per family since the base snapshot.
        // Incoming server events carry the self bit (peer 63);
        // TryDequeueEvent consumes each exactly once (see Validate), so
        // only events not yet consumed by an earlier validation remain —
        // the newest ones, which is what the last-vs-last comparison needs.
        std::unordered_map<FamilyId, SmallPayload> lastServer;
        EventLogEntry entry;
        LogCursor cursor = std::max(base->streamCursor, this->CurrentTail());
        while (this->TryDequeueEvent(63, entry, cursor))
        {
            lastServer[entry.entry.id] = entry.payload;
            cursor = entry.entry.cursor + 1;
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
                LOG_DEBUG("rollback: family {} predicted != server", cap.family);
                consistent = false;
                break;
            }
        }

        if (!consistent) RollbackToLatest(world);
        m_predictedEvents.clear();
        m_tickPredictionRanges.clear();
        return consistent;
    }

    // ---------- Rollback ----------

    void RollbackToLatest(oge::runtime::OgeRegistryRef world)
    {
        if (m_snapshots.empty()) return;

        LOG_DEBUG("rollback to tick {} (cursor {})", m_currentTick,
                  m_snapshots.back().streamCursor);
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
        m_tickPredictionRanges.clear();

        // Ask the server where it is in its log.  Validation is suspended
        // until the pong arrives; the next validation window is then
        // aligned to the tick/cursor pair the pong returns.  m_pingCursor
        // identifies the snapshot we rolled back to — the client keeps
        // pinging with it until a matching pong arrives.
        m_waitingPong = true;
        m_pingCursor = m_snapshots.back().streamCursor;
    }

    // ---------- Ping/pong alignment ----------

    bool IsWaitingPong() const
    {
        return m_waitingPong;
    }

    // The snapshot cursor the pending ping was sent with — the client
    // scene echoes it in every RollbackPing while waiting.
    LogCursor PingCursor() const
    {
        return m_pingCursor;
    }

    // Store the server's tick/cursor alignment from a RollbackPong.  The
    // pong's echoed client cursor must match the snapshot we pinged about,
    // otherwise the pong belongs to an older rollback and is dropped.
    void HandlePong(const RollbackPong& pong)
    {
        if (!m_waitingPong) return;
        if (pong.clientCursor != m_pingCursor) return;  // stale

        m_waitingPong = false;
        m_alignmentCursor = pong.serverCursor;

        // Map the server log position to a tick: look up the most recent
        // AdvanceTick entry at-or-before the pong cursor in the local log
        // (server events keep their server cursors here).  The pong is
        // applied while it is still unconsumed (validation runs after the
        // poll), and the tick is sent in the same batch as the pong, so
        // PeekEvent with the self bit finds it here.  Falls back to the
        // pong's own tick if that entry has not arrived (or has already
        // been consumed) — the pong's serverTick is the same tick anyway.
        Tick advTick = pong.serverTick;
        // The member function AdvanceTick hides the struct name inside the
        // class body — qualify it (leading :: required: the name `net` here
        // is the oge::runtime::net alias, not the enclosing namespace).
        FamilyId advanceTickId =
            entt::type_hash<::game::net::AdvanceTick>::value();
        EventLogEntryConstRef ref{{}, m_scratchPayload};
        LogCursor cursor = this->CurrentTail();
        while (this->PeekEvent(63, ref, cursor))
        {
            if (ref.entry.cursor > pong.serverCursor) break;
            if (ref.entry.id == advanceTickId)
            {
                net::Buffer b(ref.payload.data(), ref.payload.size());
                b.ToReadOnly();
                ::game::net::AdvanceTick avt{};
                net::Deserialize(b, avt);
                advTick = avt.tick;
            }
            cursor = ref.entry.cursor + 1;
        }
        m_alignmentTick = advTick;

        LOG_DEBUG("pong: server tick {} at cursor {}, alignment tick {}",
                  pong.serverTick, pong.serverCursor, m_alignmentTick);
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
        snap.tick = m_currentTick;
        snap.streamCursor = this->CurrentTail();
        for (auto& [family, cap] : m_rollbackCaps)
        {
            if (cap.takeSnapshot)
                snap.payloads[family] = cap.takeSnapshot(world);
        }
        m_snapshots.push_back(std::move(snap));
    }

    // The snapshot to validate against: the one matching the server's tick
    // (from the last pong).  The newest snapshot covers a client tick the
    // server has not necessarily reached yet, so comparing against it pairs
    // predictions with server events of the wrong window.  Without an
    // alignment (never pinged), falls back to the newest snapshot.
    const SnapshotPoint* SelectBaseSnapshot() const
    {
        const SnapshotPoint* base = &m_snapshots.back();
        if (m_alignmentTick > 0)
        {
            for (auto it = m_snapshots.rbegin(); it != m_snapshots.rend();
                 ++it)
            {
                if (it->tick <= m_alignmentTick)
                {
                    base = &(*it);
                    break;
                }
            }
        }
        return base;
    }

    void AppendTickRange()
    {
        if (m_tickPredictionRanges.empty() ||
            m_tickPredictionRanges.back().tick != m_currentTick)
        {
            m_tickPredictionRanges.push_back(
                {m_currentTick, m_predictedEvents.size() - 1, 1});
        }
        else
        {
            ++m_tickPredictionRanges.back().count;
        }
    }

    void RebuildTickRanges()
    {
        m_tickPredictionRanges.clear();
        for (size_t i = 0; i < m_predictedEvents.size(); ++i)
        {
            if (m_tickPredictionRanges.empty() ||
                m_tickPredictionRanges.back().tick !=
                    m_predictedEvents[i].tick)
            {
                m_tickPredictionRanges.push_back(
                    {m_predictedEvents[i].tick, i, 1});
            }
            else
            {
                ++m_tickPredictionRanges.back().count;
            }
        }
    }

    Tick m_currentTick = 0;
    Tick m_serverTick = 0;
    LogCursor m_serverCursor = 0;
    std::unordered_map<FamilyId, RollbackCapability> m_rollbackCaps;
    std::deque<SnapshotPoint> m_snapshots;
    std::deque<PredictedEntry> m_predictedEvents;
    std::deque<TickPredictionRange> m_tickPredictionRanges;
    mutable SmallPayload m_scratchPayload;

    // Ping/pong alignment state.
    bool m_waitingPong = false;
    LogCursor m_pingCursor = 0;  // snapshot cursor the pending ping carries
    Tick m_alignmentTick = 0;    // server tick from the last accepted pong
    LogCursor m_alignmentCursor = 0;
};

}  // namespace game::net
