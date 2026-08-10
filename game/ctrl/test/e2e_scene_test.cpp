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
#include "oge/platform/io.hpp"
#include "scene_test_harness.hpp"

namespace
{
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
    CHECK(h.waitForHandshake());

    // Let replication settle so we count only steady-state physics events.
    for (int i = 0; i < 20; ++i)
        h.poll();

    // Find the current tail cursor by scanning PeekEvent to exhaustion.
    auto& stream =
        h.serverWorld().ctx().get<game::net::EventLogStream<>>();
    game::net::LogCursor beforeCursor = 0;
    {
        std::vector<std::byte> dp;
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
            std::vector<std::byte> dp;
            game::net::EventLogEntryConstRef r{{}, dp};
            game::net::LogCursor c = beforeCursor == 0 ? 0 : beforeCursor + 1;
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
// Stability — run many frames after handshake
// =============================================================================

TEST(e2e_scenes_stability)
{
    NetSceneHarness h;
    CHECK(h.start());
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

RUN_TESTS("E2E Scene Tests")
