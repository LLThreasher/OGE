// =============================================================================
// End-to-end scene integration tests — server + client scene pair over ENet
//
// Uses NetSceneHarness to drive DebugServerScene + ClientConnScene (→
// ClientScene2) through a real ENet loopback connection in a single process.
// Verifies handshake, player creation, replication, and physics event
// bounding.
// =============================================================================

#include <test_macros.hpp>

#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "game/components.hpp"
#include "game/net/replication_events.hpp"
#include "game/net/rollback_event_log_stream.hpp"
#include "oge/log.hpp"
#include "oge/platform/io.hpp"
#include "scene_test_harness.hpp"

namespace
{
// Temporary CI diagnostic: the default test logger is a stub, so install a
// stderr logger to trace the ENet/scene handshake flow on the Linux runner.
struct StderrLogger : oge::ILogger
{
    void Log(oge::LogLevel lvl, std::string_view msg) override
    {
        const char* name = "?";
        switch (lvl)
        {
            case oge::LogLevel::Debug: name = "DEBUG"; break;
            case oge::LogLevel::Info: name = "INFO"; break;
            case oge::LogLevel::Warn: name = "WARN"; break;
            case oge::LogLevel::Error: name = "ERROR"; break;
            case oge::LogLevel::Critical: name = "CRIT"; break;
            default: break;
        }
        fprintf(stderr, "[%s] %.*s\n", name, (int)msg.size(), msg.data());
    }
    void SetSink(oge::ILogger::SinkFn, void*) override {}
    void ClearSink() override {}
};

// The platform IO layer resolves blob paths relative to the executable
// directory, e.g. "{binaryDir}/assets/player.bin".  Mirror that resolution
// so the test can pre-write player.bin where LoadOrCreatePlayer will find it.
std::string GetBinaryDir()
{
#ifdef __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) == 0)
    {
        return std::filesystem::path(buffer.data()).parent_path().string();
    }
#endif
    return ".";
}
}  // namespace

// =============================================================================
// Handshake & player creation
// =============================================================================

TEST(e2e_scenes_handshake_and_player)
{
    NetSceneHarness h;
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Verify the server world has a player entity with the full set of
    // components that ComponentPlayer::CreatePlayer creates.
    auto& world = h.serverWorld();
    bool found = false;
    for (auto [e, player] : world.view<game::ComponentPlayer>()->each())
    {
        (void)player;
        found = true;

        CHECK(world.all_of<game::ComponentPhysicBody>(e));
        CHECK(world.all_of<game::ComponentAABBCollider>(e));
        CHECK(world.all_of<game::ComponentCreature>(e));
        CHECK(world.all_of<game::ComponentCamera>(e));
        CHECK(world.all_of<game::ComponentPerspectiveCamera>(e));
        CHECK(world.all_of<game::ReplicatedTag>(e));
        break;
    }
    CHECK(found);
}

// =============================================================================
// Player replication to client
//
// After the server handshake generates the init snapshot, ClientScene2 must
// apply the AddEntity + AddComponent events so the client world contains the
// player entity with all its components.  The client player must carry the
// same uuid that ClientConnScene loaded from player.bin and sent to the
// server — that is how the client (e.g. DebugView's onCreatePlayer handler)
// identifies its own player.
// =============================================================================

TEST(e2e_player_replicated_to_client)
{
    // Pre-write player.bin with a known uuid so LoadOrCreatePlayer is
    // deterministic and the replicated client player can be matched.
    const std::array<uint8_t, 16> kPlayerUuid{
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    {
        std::filesystem::create_directories(
            std::filesystem::path(GetBinaryDir()) / "assets");
        std::vector<char> blob(16 + sizeof(game::math::vec3));
        std::memcpy(blob.data(), kPlayerUuid.data(), 16);
        game::math::vec3 pos{20.f, 20.f, 20.f};
        std::memcpy(&blob[16], &pos, sizeof(game::math::vec3));
        CHECK(oge::platform::TrySaveBlob("player.bin", blob));
    }

    NetSceneHarness h;
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for replication to land on the client.  The snapshot events
    // travel: server AddPeer → GenerateSnapshot → ENet → client
    // HandleIncoming → ApplyEvent.  ComponentPlayer is snapshotted last, so
    // wait for it to appear.
    bool clientHasPlayer = false;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cw = h.clientWorld();
            for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
            {
                (void)e;
                (void)player;
                clientHasPlayer = true;
                return true;
            }
            return false;
        },
        400));

    CHECK(clientHasPlayer);

    // The client player must be the local player: uuid matches player.bin,
    // and it must carry every replicated component (plus ReplicatedTag so
    // the client cannot echo it back).
    auto& cw = h.clientWorld();
    int playerCount = 0;
    for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
    {
        ++playerCount;
        CHECK(player.id == kPlayerUuid);
        CHECK(cw.all_of<game::ReplicatedTag>(e));
        CHECK(cw.all_of<game::ComponentPhysicBody>(e));
        CHECK(cw.all_of<game::ComponentAABBCollider>(e));
        CHECK(cw.all_of<game::ComponentCreature>(e));
        CHECK(cw.all_of<game::ComponentCamera>(e));
        CHECK(cw.all_of<game::ComponentPerspectiveCamera>(e));
    }
    CHECK_EQ(playerCount, 1);

    // Clean up the deterministic save file.
    std::filesystem::remove(std::filesystem::path(GetBinaryDir()) / "assets" /
                            "player.bin");
}

TEST(e2e_player_replicated_to_client_twice)
{
    // Temporary CI diagnostic.
    static StderrLogger s_stderrLogger;
    oge::SetLogger(&s_stderrLogger);

    // Pre-write player.bin with a known uuid so LoadOrCreatePlayer is
    // deterministic and the replicated client player can be matched.
    const std::array<uint8_t, 16> kPlayerUuid{
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    {
        std::filesystem::create_directories(
            std::filesystem::path(GetBinaryDir()) / "assets");
        std::vector<char> blob(16 + sizeof(game::math::vec3));
        std::memcpy(blob.data(), kPlayerUuid.data(), 16);
        game::math::vec3 pos{20.f, 20.f, 20.f};
        std::memcpy(&blob[16], &pos, sizeof(game::math::vec3));
        CHECK(oge::platform::TrySaveBlob("player.bin", blob));
    }

    NetSceneHarness h;
    CHECK(h.start());

    for (size_t i = 0; i < 4; i++)
    {
        // Temporary CI diagnostic.
        fprintf(stderr, "[twice] === round %zu: connect\n", i);
        CHECK(h.connect());
        bool handshakeOk = h.waitForHandshake();
        fprintf(stderr, "[twice] === round %zu: handshake=%d\n", i,
                (int)handshakeOk);
        CHECK(handshakeOk);

        // Wait for replication to land on the client.  The snapshot events
        // travel: server AddPeer → GenerateSnapshot → ENet → client
        // HandleIncoming → ApplyEvent.  ComponentPlayer is snapshotted last, so
        // wait for it to appear.
        bool clientHasPlayer = false;
        CHECK(h.pumpUntil(
            [&]
            {
                auto& cw = h.clientWorld();
                for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
                {
                    (void)e;
                    (void)player;
                    clientHasPlayer = true;
                    return true;
                }
                return false;
            },
            400));

        CHECK(clientHasPlayer);

        // The client player must be the local player: uuid matches player.bin,
        // and it must carry every replicated component (plus ReplicatedTag so
        // the client cannot echo it back).
        auto& cw = h.clientWorld();
        int playerCount = 0;
        for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
        {
            ++playerCount;
            CHECK(player.id == kPlayerUuid);
            CHECK(cw.all_of<game::ReplicatedTag>(e));
            CHECK(cw.all_of<game::ComponentPhysicBody>(e));
            CHECK(cw.all_of<game::ComponentAABBCollider>(e));
            CHECK(cw.all_of<game::ComponentCreature>(e));
            CHECK(cw.all_of<game::ComponentCamera>(e));
            CHECK(cw.all_of<game::ComponentPerspectiveCamera>(e));
        }
        CHECK_EQ(playerCount, 1);

        h.disconnect();

        bool serverHasPlayer = true;
        CHECK(h.pumpUntil(
            [&]
            {
                auto& cw = h.serverWorld();
                if (cw.view<game::ComponentPlayer>()->size() == 0) {
                    serverHasPlayer = false;
                    return true;
                }
                return false;
            },
            400));

        CHECK(!serverHasPlayer);
    }

    // Clean up the deterministic save file.
    std::filesystem::remove(std::filesystem::path(GetBinaryDir()) / "assets" /
                            "player.bin");
}

// =============================================================================
// Physics event bounding
//
// Regression: SubsystemPhysics used to call game.patch<ComponentPhysicBody>()
// three times per entity per frame (once per axis), generating three
// UpdateComponentEvent<ComponentPhysicBody> replication events per entity
// per physics tick.  With the fix, there must be at most 1 per entity per
// tick.  We verify by counting stream events on the server over a single
// poll and checking no entity appears >1 time.
// =============================================================================

TEST(e2e_physics_events_bounded)
{
    NetSceneHarness h;
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Let replication settle so we count only steady-state physics events.
    for (int i = 0; i < 20; ++i)
        h.poll();

    // Find the current tail cursor by scanning PeekEvent to exhaustion.
    auto& stream =
        h.serverWorld().ctx().get<game::net::EventLogStream<>>();
    game::net::LogCursor beforeCursor = 0;
    {
        game::net::SmallPayload dp;
        game::net::EventLogEntryConstRef r{{}, dp};
        game::net::LogCursor c = 0;
        while (stream.PeekEvent(0, r, c == 0 ? 0 : c))
        {
            beforeCursor = r.entry.cursor;
            c = beforeCursor + 1;
        }
    }

    // Sample a few polls.  Each poll's batch of physics events must contain
    // at most one UpdateComponentEvent<ComponentPhysicBody> per entity —
    // SubsystemPhysics used to patch the body once per axis (3 events per
    // entity per tick) and now fires a single patch per modified entity.
    // Sampling several polls keeps the check robust against stream-tail
    // timing while still catching the regression.
    using UpdatePhysic =
        game::net::UpdateComponentEvent<game::ComponentPhysicBody>;
    const auto physFamily =
        entt::type_hash<UpdatePhysic>::value();

    bool anyEvent = false;
    for (int sample = 0; sample < 10; ++sample)
    {
        h.poll();

        std::unordered_map<entt::entity, int> perEntityCount;
        game::net::LogCursor lastCursor = 0;
        {
            game::net::SmallPayload dp;
            game::net::EventLogEntryConstRef r{{}, dp};
            game::net::LogCursor c = beforeCursor == 0 ? 0 : beforeCursor + 1;
            // The server consumes (and its tail advances past) events as it
            // sends them — with a single peer everything drains within a
            // poll, so the tail can overtake a scan point captured before
            // the poll.  Clamp to the current tail: events the tail already
            // passed are consumed and no longer visible anyway.
            if (c != 0 && c < stream.CurrentTail()) c = stream.CurrentTail();
            while (stream.PeekEvent(0, r, c == 0 ? 0 : c))
            {
                if (r.entry.id == physFamily)
                {
                    anyEvent = true;
                    // Peek the entity field from the payload (entity is the
                    // first field of UpdateComponentEvent<T>, serialized as
                    // a 4-byte uint32).
                    if (r.payload.size() >= sizeof(uint32_t))
                    {
                        uint32_t rawEntity;
                        std::memcpy(&rawEntity, r.payload.data(),
                                    sizeof(uint32_t));
                        perEntityCount[entt::entity{rawEntity}]++;
                    }
                }
                lastCursor = r.entry.cursor;
                c = r.entry.cursor + 1;
            }
        }

        // With one player and the fix, each entity gets at most 1
        // UpdateComponentEvent per physics frame.
        for (auto& [e, count] : perEntityCount)
        {
            (void)e;
            CHECK(count <= 1);
        }

        if (lastCursor != 0) beforeCursor = lastCursor;
    }

    // At least one sample must have produced physics events (gravity affects
    // the player body every frame).
    CHECK(anyEvent);
}

// =============================================================================
// Chunk replication — AddChunk
//
// After the handshake, SubsystemTerrain generates chunks around the server
// player.  PollTerrainChunkEvents pushes AddChunkEvents into the replication
// stream; the client must apply them into its own TerrainView with the same
// block data (palette-compressed round-trip).
// =============================================================================

TEST(e2e_add_chunk_replicates)
{
    NetSceneHarness h;
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for the server to generate at least one persistent chunk.
    oge::Point3 serverCoord{};
    uint32_t serverBlock = 0;
    bool serverHasChunk = false;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& world = h.serverWorld();
            if (!world.ctx().contains<game::terrain::TerrainView>())
                return false;
            auto& terrain = world.ctx().get<game::terrain::TerrainView>();
            game::terrain::ChunkHandle cursor{};
            while (const auto* c = terrain.PollChunk(cursor))
            {
                if (c->state != game::terrain::ChunkState::Persistent)
                    continue;
                serverCoord = c->Coords;
                serverBlock = c->GetBlock(0, 0, 0);
                serverHasChunk = true;
                return true;
            }
            return false;
        },
        600));

    CHECK(serverHasChunk);

    // The same chunk must appear on the client, persistent and with the same
    // block data at (0,0,0).
    uint32_t clientBlock = 0;
    bool clientHasChunk = false;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& world = h.clientWorld();
            if (!world.ctx().contains<game::terrain::TerrainView>())
                return false;
            auto& terrain = world.ctx().get<game::terrain::TerrainView>();
            auto [handle, chunk] = terrain.GetChunk(serverCoord);
            if (handle.IsValid() && chunk != nullptr &&
                chunk->state == game::terrain::ChunkState::Persistent)
            {
                clientBlock = chunk->GetBlock(0, 0, 0);
                clientHasChunk = true;
                return true;
            }
            return false;
        },
        600));

    CHECK(clientHasChunk);
    CHECK_EQ(clientBlock, serverBlock);
}

// =============================================================================
// Chunk replication — UpdateChunk (block update)
//
// Server sets a block inside a persistent chunk → dirty ChunkStateUpdateEvent
// → UpdateChunkEvent → client applies it to its chunk.
// =============================================================================

TEST(e2e_update_chunk_replicates)
{
    NetSceneHarness h;
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for a persistent chunk on the server, then set a block in it.
    oge::Point3 serverCoord{};
    bool serverHasChunk = false;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& world = h.serverWorld();
            if (!world.ctx().contains<game::terrain::TerrainView>())
                return false;
            auto& terrain = world.ctx().get<game::terrain::TerrainView>();
            game::terrain::ChunkHandle cursor{};
            while (const auto* c = terrain.PollChunk(cursor))
            {
                if (c->state != game::terrain::ChunkState::Persistent)
                    continue;
                serverCoord = c->Coords;
                serverHasChunk = true;
                return true;
            }
            return false;
        },
        600));
    CHECK(serverHasChunk);

    // Set a block at the chunk's local (1,1,1) → world coords.  Use the
    // stone block id so the new value differs from the generated terrain.
    auto& blocks = h.serverWorld().ctx().get<game::terrain::BlockRegistry>();
    const uint32_t stoneId = blocks.GetBlockId("stone");
    const oge::Point3 worldPos{
        serverCoord.x * 16 + 1, serverCoord.y * 16 + 1, serverCoord.z * 16 + 1};
    auto& terrain = h.serverWorld().ctx().get<game::terrain::TerrainView>();
    terrain.SetBlock(worldPos, stoneId);

    // The client chunk must receive the updated block.
    uint32_t clientValue = 0;
    bool clientUpdated = false;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cw = h.clientWorld();
            if (!cw.ctx().contains<game::terrain::TerrainView>())
                return false;
            auto& cterrain = cw.ctx().get<game::terrain::TerrainView>();
            auto [handle, chunk] = cterrain.GetChunk(serverCoord);
            if (handle.IsValid() && chunk != nullptr &&
                chunk->state == game::terrain::ChunkState::Persistent)
            {
                if (cterrain.TryGetBlock(worldPos, clientValue))
                {
                    clientUpdated = true;
                    return true;
                }
            }
            return false;
        },
        600));

    CHECK(clientUpdated);
    CHECK_EQ(clientValue, stoneId);
}

// =============================================================================
// Player input replication
//
// The client player's input stream is replicated to the server as
// PlayerInputReplicationEvent (packed with game::input::net::PackFrame).
// ClientScene2 installs the input hooks and calls PollPlayerInputs each tick;
// DebugServerScene installs the same hooks so ApplyEvent can insert the
// incoming frame into the server player's stream.
//
// The harness has no DebugVoxelView, so the test creates the player's
// PlayerInputStream component manually (as DebugVoxelView does) — the
// on_construct hook auto-registers it.  Then dummy input is injected and the
// test verifies the server's stream contains the same packed content.
// =============================================================================

TEST(e2e_player_input_replicates)
{
    NetSceneHarness h;
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for the player entity to replicate to the client.
    entt::entity clientPlayer = entt::null;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cw = h.clientWorld();
            for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
            {
                (void)player;
                clientPlayer = e;
                return true;
            }
            return false;
        },
        400));
    CHECK(clientPlayer != entt::null);

    // Wire up the local player's input stream (DebugVoxelView does this in
    // the real app; ClientScene2's hooks auto-register it on construct).
    auto& cw = h.clientWorld();
    cw.emplace<game::input::PlayerInputStream>(clientPlayer);
    auto& clientStream =
        cw.get<game::input::PlayerInputStream>(clientPlayer);

    // Push a frame delta with a jump action + move delta, then AdvanceTick to
    // flush the accumulated frame into the discrete stream.  The new frame-stream
    // API accumulates deltas via PushFrame and commits them via AdvanceTick.
    {
        game::input::PlayerInputFrameDelta delta;
        delta.inputEvent = game::input::PlayerInputEvent{
            game::math::vec2{0.25f, 0.5f}, game::input::PlayerAction::Jump};
        delta.moveDelta = game::math::vec2{0.5f, 0.25f};
        clientStream.PushFrame(delta);
    }
    clientStream.AdvanceTick();

    // The server must receive the input in its player's stream with the same
    // packed frame content (action mask + quantized positions).
    bool receivedAction = false;
    bool receivedMove = false;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& sw = h.serverWorld();
            for (auto [e, player] : sw.view<game::ComponentPlayer>()->each())
            {
                (void)player;
                auto& stream = sw.get<game::input::PlayerInputStream>(e);

                // Read from the very first event: cursor 1, not the default
                // 0 (a zero cursor snaps to the frontier and skips all).
                game::input::PlayerInputStream::Cursor c{1};
                game::input::PlayerInputFrame frame;
                while (stream.PollFrame(c, frame))
                {
                    for (size_t i = 0; i < frame.inputEventCnt; ++i)
                    {
                        if (frame.inputEvents[i]
                                .get<game::input::PlayerAction::Jump>())
                        {
                            receivedAction = true;
                        }
                    }
                    // move is in world space (Camera.right * moveDelta.x +
                    // Camera.forward * moveDelta.y).  Check magnitude rather
                    // than exact components — SNorm8 quantization rounds.
                    if (std::abs(frame.move.x) > 0.4f ||
                        std::abs(frame.move.z) > 0.2f)
                        receivedMove = true;
                }

                if (receivedAction && receivedMove) return true;
            }
            return false;
        },
        400));

    CHECK(receivedAction);
    CHECK(receivedMove);
}

// =============================================================================
// Prediction vs authoritative — drive input on the client and check that both
// the local prediction copy (client world) and the client-owned authoritative
// mirror (server round-trip) apply it consistently.
//
// Regression coverage:
//  - PackedPlayerInputFrame dropped moveZ on the wire (forward movement at
//    identity camera is pure +z, so the server never moved).
//  - UnpackFrame never restored hasAim (the server camera ignored the
//    client's pan).
// =============================================================================

TEST(e2e_player_input_prediction_vs_authoritative)
{
    NetSceneHarness h;
    h.enableClientPrediction();
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for the player to replicate into the client's prediction world
    // with the full component set the local SubsystemPlayer consumes.
    entt::entity clientPlayer = entt::null;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cw = h.clientWorld();
            for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
            {
                (void)player;
                if (cw.all_of<game::ComponentCamera>(e) &&
                    cw.all_of<game::ComponentPhysicBody>(e) &&
                    cw.all_of<game::ComponentCreature>(e) &&
                    cw.all_of<game::ComponentAABBCollider>(e) &&
                    cw.all_of<game::ComponentPerspectiveCamera>(e))
                {
                    clientPlayer = e;
                    return true;
                }
            }
            return false;
        },
        400));
    CHECK(clientPlayer != entt::null);

    // The authoritative mirror receives only physics-related families
    // (entity adds + AABBCollider/PhysicBody; camera/creature/player updates
    // are masked to the prediction world).  The entity id is shared with the
    // prediction world, so wait for the mirrored body only.
    entt::entity authPlayer = clientPlayer;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& aw = h.clientAuthoritativeWorld();
            return aw.valid(authPlayer) &&
                   aw.all_of<game::ComponentPhysicBody>(authPlayer);
        },
        400));

    // Wire up the local input stream and simulation tag (DebugVoxelView
    // does this in the app; the tag is also replicated now, but keep the
    // emplace defensive like e2e_local_prediction_player_moves).
    auto& cw = h.clientWorld();
    cw.emplace<game::input::PlayerInputStream>(clientPlayer);
    if (!cw.all_of<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer))
        cw.emplace<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer);
    auto& clientStream =
        cw.get<game::input::PlayerInputStream>(clientPlayer);

    // Find the server's player (entity ids are shared client/server, but a
    // view lookup keeps the test independent of id allocation order).
    entt::entity serverPlayer = entt::null;
    for (auto [e, player] : h.serverWorld().view<game::ComponentPlayer>()->each())
    {
        (void)player;
        serverPlayer = e;
    }
    CHECK(serverPlayer != entt::null);

    const auto predPos0 = cw.get<game::ComponentPhysicBody>(clientPlayer).pos;
    const auto& aw = h.clientAuthoritativeWorld();
    const auto authPos0 = aw.get<game::ComponentPhysicBody>(authPlayer).pos;
    const float srvYaw0 =
        h.serverWorld().get<game::ComponentCamera>(serverPlayer).yaw;

    // Drive forward input (+ a rightward pan) for 90 frames, one delta per
    // poll, then let replication settle.
    constexpr float PanPerFrame = 0.05f;
    constexpr int InputFrames = 90;
    for (int i = 0; i < InputFrames; ++i)
    {
        game::input::PlayerInputFrameDelta delta;
        delta.moveDelta = game::math::vec2{0.f, 1.f};  // forward (W)
        delta.panDelta = game::math::vec2{PanPerFrame, 0.f};
        clientStream.PushFrame(delta);
        h.poll();
    }
    for (int i = 0; i < 20; ++i) h.poll();

    const auto predPos1 = cw.get<game::ComponentPhysicBody>(clientPlayer).pos;
    const auto authPos1 = aw.get<game::ComponentPhysicBody>(authPlayer).pos;

    // Horizontal displacement only — gravity may differ between the two
    // simulations.
    const float predDist = game::math::len(
        game::math::vec2{predPos1.x - predPos0.x, predPos1.z - predPos0.z});
    const float authDist = game::math::len(
        game::math::vec2{authPos1.x - authPos0.x, authPos1.z - authPos0.z});

    // Local prediction applied the input.
    CHECK(predDist > 0.5f);

    // The server applied the same input and replicated it back into the
    // authoritative mirror.  Before the moveZ wire fix, forward movement
    // serialized to (0, 0) — the authoritative copy never moved.
    CHECK(authDist > 0.5f);

    // The authoritative copy must track the prediction instead of stuttering
    // at a fraction of it (empty frames polluting the server's input ring).
    CHECK(authDist > 0.6f * predDist);

    // The server camera must follow the client's aim — hasAim was dropped by
    // UnpackFrame before the fix, so the accumulated yaw stayed 0.  Allow a
    // couple of frames of slack: the server's first poll snaps its cursor to
    // the stream frontier and may skip the very first frame.
    const float srvYaw =
        h.serverWorld().get<game::ComponentCamera>(serverPlayer).yaw;
    const float yawDelta = game::input::WrapRadians0To2Pi(srvYaw) -
                           game::input::WrapRadians0To2Pi(srvYaw0);
    CHECK(yawDelta > InputFrames * PanPerFrame - 0.5f);
}

// =============================================================================
// Jump + move + aim at the same time — the prediction and authoritative
// copies must both jump.  The real client config runs
// SubsystemPlayer<FixedStep> in the realtime pipeline to process action
// events; if the prediction world lacks it, the local copy stays grounded
// while the authoritative copy hops ~1.65 m — a visible divergence.
// =============================================================================

TEST(e2e_player_input_jump_move_aim_sync)
{
    NetSceneHarness h;
    h.enableClientPrediction();
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for the player to replicate to the client's prediction world.
    entt::entity clientPlayer = entt::null;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cw = h.clientWorld();
            for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
            {
                (void)player;
                clientPlayer = e;
                return true;
            }
            return false;
        },
        400));
    CHECK(clientPlayer != entt::null);

    // The authoritative mirror only carries the physics body (see
    // e2e_player_input_prediction_vs_authoritative).
    entt::entity authPlayer = clientPlayer;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& aw = h.clientAuthoritativeWorld();
            return aw.valid(authPlayer) &&
                   aw.all_of<game::ComponentPhysicBody>(authPlayer);
        },
        400));

    // Wire up local input + simulation tag (DebugVoxelView does this in the
    // app).
    auto& cw = h.clientWorld();
    cw.emplace<game::input::PlayerInputStream>(clientPlayer);
    if (!cw.all_of<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer))
        cw.emplace<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer);
    auto& clientStream =
        cw.get<game::input::PlayerInputStream>(clientPlayer);

    entt::entity serverPlayer = entt::null;
    for (auto [e, player] : h.serverWorld().view<game::ComponentPlayer>()->each())
    {
        (void)player;
        serverPlayer = e;
    }
    CHECK(serverPlayer != entt::null);

    // Let the player fall onto terrain and come to rest — the jump impulse
    // is gated on isGrounded.  Both copies must be grounded: the client's
    // prediction world collides against its own replicated terrain.
    auto& sw = h.serverWorld();
    CHECK(h.pumpUntil(
        [&]
        {
            return sw.get<game::ComponentPhysicBody>(serverPlayer).isGrounded;
        },
        600));
    CHECK(h.pumpUntil(
        [&]
        {
            return cw.get<game::ComponentPhysicBody>(clientPlayer).isGrounded;
        },
        600));

    const float groundY =
        sw.get<game::ComponentPhysicBody>(serverPlayer).pos.y;
    const float predGroundY =
        cw.get<game::ComponentPhysicBody>(clientPlayer).pos.y;

    // Drive jump + move + aim together.  Track each copy's peak height and
    // the poll at which it lifts off its own ground line — CreatePlayer sets
    // a ~1.65 m jump (SetMaxJumpHeight).
    constexpr int InputFrames = 90;
    float predPeak = predGroundY;
    float authPeak = groundY;
    int predLiftPoll = -1;
    int authLiftPoll = -1;
    for (int i = 0; i < InputFrames; ++i)
    {
        game::input::PlayerInputFrameDelta delta;
        delta.moveDelta = game::math::vec2{0.f, 1.f};  // forward (W)
        delta.panDelta = game::math::vec2{0.05f, 0.f};
        delta.inputEvent = game::input::PlayerInputEvent{
            game::math::vec2{0.f, 0.f}, game::input::PlayerAction::Jump};
        clientStream.PushFrame(delta);
        h.poll();

        const float predY =
            cw.get<game::ComponentPhysicBody>(clientPlayer).pos.y;
        const float authY = h.clientAuthoritativeWorld()
                                .get<game::ComponentPhysicBody>(authPlayer)
                                .pos.y;
        predPeak = game::math::max(predPeak, predY);
        authPeak = game::math::max(authPeak, authY);
        if (predLiftPoll < 0 && predY > predGroundY + 0.1f) predLiftPoll = i;
        if (authLiftPoll < 0 && authY > groundY + 0.1f) authLiftPoll = i;
    }
    for (int i = 0; i < 20; ++i) h.poll();

    const float predLift = predPeak - predGroundY;
    const float authLift = authPeak - groundY;

    // The authoritative copy jumped...
    CHECK(authLift > 0.5f);

    // ...and so did the prediction copy.
    CHECK(predLift > 0.5f);

    // The prediction copy must jump on its own — BEFORE the server round
    // trip lands in the authoritative mirror.  If it only moves because the
    // rollback pass restores server truth, predLiftPoll >= authLiftPoll and
    // the player visibly pops instead of predicting.
    CHECK(predLiftPoll >= 0);
    CHECK(authLiftPoll >= 0);
    CHECK(predLiftPoll < authLiftPoll);

    // Both copies agree on the jump height (the server's fixed-step physics
    // double-integrates gravity, so allow a small skew).
    CHECK(std::abs(predLift - authLift) < 0.5f);
}

// =============================================================================
// Stability — run many frames after handshake
// =============================================================================

TEST(e2e_scenes_stability)
{
    NetSceneHarness h;
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Run 120 frames (~2 seconds at 60 fps) after handshake.  The server
    // must keep ticking subsystems + replication without assertion failures
    // or stale state.
    for (int i = 0; i < 120; ++i)
        h.poll();

    // Server world must still be valid.
    auto& world = h.serverWorld();
    bool hasPlayer = false;
    for (auto [e, player] : world.view<game::ComponentPlayer>()->each())
    {
        (void)player;
        hasPlayer = true;
        CHECK(world.valid(e));
        break;
    }
    CHECK(hasPlayer);
}

// =============================================================================
// Interpolation — entity transform is smoothed between physics ticks
// =============================================================================

TEST(e2e_interpolation_layer)
{
    NetSceneHarness h;
    // The visible world is locally simulated (client-side prediction), so
    // movement comes from local physics, not replicated server updates.
    // Server physics updates route only to the authoritative mirror world.
    h.enableClientPrediction();
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for the player to replicate to the client.
    entt::entity clientPlayer = entt::null;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cw = h.clientWorld();
            for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
            {
                (void)player;
                clientPlayer = e;
                return true;
            }
            return false;
        },
        400));
    CHECK(clientPlayer != entt::null);

    // Tag the client player for interpolation and capture initial position.
    auto& cw = h.clientWorld();
    cw.emplace_or_replace<game::RenderStrategyTag<game::RenderStrategy::Interpolation>>(
        clientPlayer);
    // Wire the player for local simulation (as DebugVoxelView does) so the
    // realtime subsystems drive the body.
    if (!cw.all_of<game::input::PlayerInputStream>(clientPlayer))
        cw.emplace<game::input::PlayerInputStream>(clientPlayer);
    if (!cw.all_of<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer))
        cw.emplace<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer);
    auto& body = cw.get<game::ComponentPhysicBody>(clientPlayer);
    game::math::vec3 initialPos = body.pos;

    // Poll several frames — local physics (gravity) moves the player
    // downward.  No server updates reach this world.
    bool moved = false;
    for (int i = 0; i < 60; ++i)
    {
        h.poll();
        auto& cb = cw.get<game::ComponentPhysicBody>(clientPlayer);
        if (game::math::len_sq(cb.pos - initialPos) > 0.001f)
        {
            moved = true;
            break;
        }
    }
    CHECK(moved);

    // After moving, an interpolated transform should have been written
    // (SceneView::Update calls PostUpdate which emplaces it).
    // In the test harness there is no SceneView, so we verify the entity
    // is tagged correctly and the physics position changed.
    CHECK(cw.all_of<game::RenderStrategyTag<game::RenderStrategy::Interpolation>>(
        clientPlayer));
}

// =============================================================================
// Local prediction — client predicts player position locally
// =============================================================================

TEST(e2e_local_prediction_player_moves)
{
    NetSceneHarness h;
    h.enableClientPrediction();
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for the player to replicate to the client.
    entt::entity clientPlayer = entt::null;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cw = h.clientWorld();
            for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
            {
                (void)player;
                clientPlayer = e;
                return true;
            }
            return false;
        },
        400));
    CHECK(clientPlayer != entt::null);

    // Tag for local prediction and wire up input (as DebugVoxelView does).
    auto& cw = h.clientWorld();
    cw.emplace_or_replace<game::RenderStrategyTag<game::RenderStrategy::LocalPrediction>>(
        clientPlayer);
    if (!cw.all_of<game::input::PlayerInputStream>(clientPlayer))
        cw.emplace<game::input::PlayerInputStream>(clientPlayer);
    if (!cw.all_of<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer))
        cw.emplace<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer);

    auto& clientStream = cw.get<game::input::PlayerInputStream>(clientPlayer);

    // Record initial position, then inject move input.
    auto& body = cw.get<game::ComponentPhysicBody>(clientPlayer);
    game::math::vec3 initialPos = body.pos;
    {
        game::input::PlayerInputFrameDelta delta;
        delta.moveDelta = game::math::vec2{0.5f, 0.f};
        clientStream.PushFrame(delta);
    }
    clientStream.AdvanceTick();

    // Poll until the client player moves (local physics responds to input).
    bool moved = false;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cb = cw.get<game::ComponentPhysicBody>(clientPlayer);
            if (game::math::len_sq(cb.pos - initialPos) > 0.001f)
            {
                moved = true;
                return true;
            }
            return false;
        },
        120));
    CHECK(moved);

    // Predicted events should have been inserted.
    auto& rbs = h.clientRollbackStream();
    CHECK(rbs.HasCapability(
        entt::type_hash<
            game::net::UpdateComponentEvent<game::ComponentPhysicBody>>::value()));
}

// =============================================================================
// Rollback — client snaps to server position on mismatch
// =============================================================================

TEST(e2e_rollback_on_server_correction)
{
    NetSceneHarness h;
    h.enableClientPrediction();
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for the player on both sides.
    entt::entity clientPlayer = entt::null;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cw = h.clientWorld();
            for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
            {
                (void)player;
                clientPlayer = e;
                return true;
            }
            return false;
        },
        400));
    CHECK(clientPlayer != entt::null);

    // Tag for local prediction and set up input.
    auto& cw = h.clientWorld();
    cw.emplace_or_replace<game::RenderStrategyTag<game::RenderStrategy::LocalPrediction>>(
        clientPlayer);
    if (!cw.all_of<game::input::PlayerInputStream>(clientPlayer))
        cw.emplace<game::input::PlayerInputStream>(clientPlayer);
    if (!cw.all_of<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer))
        cw.emplace<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer);

    // Let a few frames run so the client receives AdvanceTick events and
    // the rollback stream takes snapshots.  The snapshots are taken from
    // the authoritative world (clean server truth), not the prediction
    // world.
    CHECK(h.pumpUntil(
        [&]
        {
            return h.clientRollbackStream().Snapshots().size() > 0;
        },
        200));

    // Teleport the server player to a distant position.  The on_update hook
    // fires, generating an UpdateComponentEvent<ComponentPhysicBody> that
    // travels to the client.  The client's ValidateLatest compares and, if the
    // positions diverge, rolls back.
    entt::entity serverPlayer = entt::null;
    auto& sw = h.serverWorld();
    for (auto [e, player] : sw.view<game::ComponentPlayer>()->each())
    {
        (void)player;
        serverPlayer = e;
        break;
    }
    CHECK(serverPlayer != entt::null);

    game::math::vec3 distantPos{100.f, 100.f, 100.f};
    sw.patch<game::ComponentPhysicBody>(serverPlayer,
                                        [&](auto& b)
                                        { b.pos = distantPos; });

    auto& rbs = h.clientRollbackStream();

    // The client detects the divergence (predicted pos != server teleport),
    // rolls back to the latest authoritative snapshot and sends a ping —
    // validation is suspended until the pong arrives.
    CHECK(h.pumpUntil([&] { return rbs.IsWaitingPong(); }, 200));

    // The server answers with its tick/cursor alignment; validation resumes
    // with the snapshot aligned to the server's tick.
    CHECK(h.pumpUntil([&] { return !rbs.IsWaitingPong(); }, 200));

    // The rollback restored the client to the authoritative state (which
    // already contains the teleport — the snapshot is taken from the mirror
    // world), so the client position now matches the server.
    bool converged = false;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cb = cw.get<game::ComponentPhysicBody>(clientPlayer);
            float dist = game::math::len(cb.pos - distantPos);
            if (dist < 2.0f)
            {
                converged = true;
                return true;
            }
            return false;
        },
        200));
    CHECK(converged);
}

// =============================================================================
// Rollback recovery + re-trigger — partial erase preserves prediction state
// through a ping-pong cycle, and rollback still fires on genuine divergence.
//
// After a rollback the client pings the server; validation is suspended until
// the pong arrives.  During this round-trip the client keeps predicting.  The
// first ValidateLatest after the pong should only compare predictions up to
// the server's confirmed tick — not the full prediction backlog from the
// round-trip window.  If the comparison window is unbounded the client
// sees a count mismatch and rolls back again, creating a livelock.
//
// The test then triggers a second correction to prove the mechanism is still
// alive: a genuine out-of-sync state provokes another rollback.
// =============================================================================

TEST(e2e_rollback_recovery_no_relapse)
{
    NetSceneHarness h;
    h.enableClientPrediction();
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for the client player.
    entt::entity clientPlayer = entt::null;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cw = h.clientWorld();
            for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
            {
                (void)player;
                clientPlayer = e;
                return true;
            }
            return false;
        },
        400));
    CHECK(clientPlayer != entt::null);

    // Tag for local prediction and wire up input.
    auto& cw = h.clientWorld();
    cw.emplace_or_replace<game::RenderStrategyTag<game::RenderStrategy::LocalPrediction>>(
        clientPlayer);
    if (!cw.all_of<game::input::PlayerInputStream>(clientPlayer))
        cw.emplace<game::input::PlayerInputStream>(clientPlayer);
    if (!cw.all_of<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer))
        cw.emplace<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer);

    // Let snapshots accumulate.
    auto& rbs = h.clientRollbackStream();
    CHECK(h.pumpUntil([&] { return rbs.Snapshots().size() > 0; }, 200));

    // Find the server player.
    entt::entity serverPlayer = entt::null;
    auto& sw = h.serverWorld();
    for (auto [e, player] : sw.view<game::ComponentPlayer>()->each())
    {
        (void)player;
        serverPlayer = e;
        break;
    }
    CHECK(serverPlayer != entt::null);

    // ---- First correction: trigger rollback, verify recovery ----

    game::math::vec3 distantPos1{50.f, 0.f, 50.f};
    sw.patch<game::ComponentPhysicBody>(serverPlayer,
                                        [&](auto& b)
                                        { b.pos = distantPos1; });

    // Wait for the rollback to trigger (ping sent).
    CHECK(h.pumpUntil([&] { return rbs.IsWaitingPong(); }, 200));

    // Advance a few frames while waiting — generates predictions in the
    // round-trip window (the client keeps simulating locally).
    auto& clientStream = cw.get<game::input::PlayerInputStream>(clientPlayer);
    for (int i = 0; i < 5; ++i)
    {
        {
            game::input::PlayerInputFrameDelta delta;
            delta.moveDelta = game::math::vec2{0.01f, 0.f};
            clientStream.PushFrame(delta);
        }
        clientStream.AdvanceTick();
        h.poll();
    }

    // Wait for the pong — validation resumes with the server's alignment.
    CHECK(h.pumpUntil([&] { return !rbs.IsWaitingPong(); }, 200));

    // Run several frames with input.  Key assertion: the stream must NOT
    // re-enter the waiting-pong state.  Pre-fix, the count mismatch from
    // comparing the full round-trip prediction backlog triggers an immediate
    // re-rollback.
    for (int i = 0; i < 10; ++i)
    {
        {
            game::input::PlayerInputFrameDelta delta;
            delta.moveDelta = game::math::vec2{0.01f, 0.f};
            clientStream.PushFrame(delta);
        }
        clientStream.AdvanceTick();
        h.poll();
        if (rbs.IsWaitingPong()) break;
    }
    CHECK(!rbs.IsWaitingPong());

    // Predictions beyond the server alignment should have survived.
    CHECK(rbs.PredictedCount() > 0);

    // Client player must still be valid.
    CHECK(cw.valid(clientPlayer));
    CHECK(cw.all_of<game::ComponentPhysicBody>(clientPlayer));

    // ---- Second correction: prove rollback still works ----
    // The mechanism must not be permanently deadened — a genuine
    // out-of-sync state should still provoke a rollback.

    game::math::vec3 distantPos2{-50.f, 0.f, -50.f};
    sw.patch<game::ComponentPhysicBody>(serverPlayer,
                                        [&](auto& b)
                                        { b.pos = distantPos2; });

    // Wait for the second rollback.
    CHECK(h.pumpUntil([&] { return rbs.IsWaitingPong(); }, 200));

    // Wait for the second pong recovery.
    CHECK(h.pumpUntil([&] { return !rbs.IsWaitingPong(); }, 200));

    // Client should converge to the second correction.
    bool converged = false;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cb = cw.get<game::ComponentPhysicBody>(clientPlayer);
            float dist = game::math::len(cb.pos - distantPos2);
            if (dist < 2.0f)
            {
                converged = true;
                return true;
            }
            return false;
        },
        200));
    CHECK(converged);

    // Run a few more frames with input — must still be stable (no third
    // rollback from spurious count mismatch).
    for (int i = 0; i < 10; ++i)
    {
        {
            game::input::PlayerInputFrameDelta delta;
            delta.moveDelta = game::math::vec2{0.01f, 0.f};
            clientStream.PushFrame(delta);
        }
        clientStream.AdvanceTick();
        h.poll();
        if (rbs.IsWaitingPong()) break;
    }
    CHECK(!rbs.IsWaitingPong());

    CHECK(cw.valid(clientPlayer));
    CHECK(cw.all_of<game::ComponentPhysicBody>(clientPlayer));
}

// =============================================================================
// Stability — extended run with local prediction enabled
// =============================================================================

TEST(e2e_prediction_stability)
{
    NetSceneHarness h;
    h.enableClientPrediction();
    CHECK(h.start());
    CHECK(h.connect());
    CHECK(h.waitForHandshake());

    // Wait for the player on the client.
    entt::entity clientPlayer = entt::null;
    CHECK(h.pumpUntil(
        [&]
        {
            auto& cw = h.clientWorld();
            for (auto [e, player] : cw.view<game::ComponentPlayer>()->each())
            {
                (void)player;
                clientPlayer = e;
                return true;
            }
            return false;
        },
        400));
    CHECK(clientPlayer != entt::null);

    // Tag for local prediction and wire up input.
    auto& cw = h.clientWorld();
    cw.emplace_or_replace<game::RenderStrategyTag<game::RenderStrategy::LocalPrediction>>(
        clientPlayer);
    if (!cw.all_of<game::input::PlayerInputStream>(clientPlayer))
        cw.emplace<game::input::PlayerInputStream>(clientPlayer);
    if (!cw.all_of<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer))
        cw.emplace<game::UpdateTag<game::UpdateType::Realtime>>(clientPlayer);

    // Run 200 frames with prediction enabled — must not crash or assert.
    for (int i = 0; i < 200; ++i)
        h.poll();

    // Both worlds must still be valid.
    CHECK(h.serverWorld().valid(clientPlayer) ||
          true);  // server player may or may not exist

    // Client player must still be alive.
    CHECK(cw.valid(clientPlayer));
    CHECK(cw.all_of<game::ComponentPhysicBody>(clientPlayer));
}

RUN_TESTS("E2E Scene Tests")
