#include <cassert>
#include <cstddef>
#include <cstdint>

#include "game/components.hpp"
#include "game/input/player_input_stream.hpp"
#include "game/replication_registry.hpp"
#include "game/terrain/defs.hpp"
#include "game/terrain/terrain_view.hpp"
#include "oge/fmt.hpp"
#include "oge/log.hpp"
#include "oge/point3.hpp"
#include "oge/runtime/net_packet_sender.hpp"
#include "oge/runtime/typed_registry.hpp"

using oge::runtime::AnythingFactory;
using oge::runtime::SendType;

static void RegisterEntityReplication(AnythingFactory& af)
{
    auto& desc = af.RegisterType<game::ReplicatedTag>();
    desc.capabilities.template Add<game::ReplicationCapability>(
        af.Id<game::ReplicatedTag>(), &game::EntityReplication::Encode,
        &game::EntityReplication::Decode, &game::EntityReplication::CreateState,
        SendType::Reliable, 0);
}

static void RegisterTerrainReplication(AnythingFactory& af)
{
    auto& desc = af.RegisterType<game::terrain::TerrainView>();
    desc.capabilities.template Add<game::ReplicationCapability>(
        af.Id<game::terrain::TerrainView>(), &game::TerrainReplication::Encode,
        &game::TerrainReplication::Decode,
        &game::TerrainReplication::CreateState, SendType::Reliable, 2);
}

template <typename T>
static void RegisterEventReplication(AnythingFactory& af)
{
    auto& desc = af.RegisterType<T>();
    desc.capabilities.template Add<game::ReplicationCapability>(
        af.Id<T>(), &game::EntityEventStreamReplication<T>::Encode,
        &game::EntityEventStreamReplication<T>::Decode,
        &game::EntityEventStreamReplication<T>::CreateState, SendType::Reliable,
        0);
}

template <typename T>
static void RegisterComponentReplication(AnythingFactory& af)
{
    auto& desc = af.RegisterType<T>();
    desc.capabilities.template Add<game::ReplicationCapability>(
        af.Id<T>(), &game::ComponentReplication<T>::Encode,
        &game::ComponentReplication<T>::Decode,
        &game::ComponentReplication<T>::CreateState, SendType::Reliable, 0);
}

void game::RegisterReplications(AnythingFactory& af, ReplicationRegistry& rf)
{
    RegisterTerrainReplication(af);
    RegisterEntityReplication(af);
    RegisterComponentReplication<ComponentAABBCollider>(af);
    RegisterComponentReplication<ComponentCamera>(af);
    RegisterComponentReplication<ComponentPerspectiveCamera>(af);
    RegisterComponentReplication<ComponentCreature>(af);
    RegisterComponentReplication<ComponentPlayer>(af);
    RegisterEventReplication<input::PlayerInputStream>(af);
    rf.RegisterFrom(af);
}

void game::InstallTerrainReplicationHooks(entt::registry& world)
{
}

void game::InstallEntityReplicationHooks(entt::registry& world)
{
    world.ctx().emplace<EntityEventStream>();
    world.on_construct<ReplicatedTag>()
        .template connect<+[](entt::registry& world, entt::entity e)
                          {
                              LOG_DEBUG("create entity {}", (uint64_t)e);
                              world.ctx()
                                  .template get<input::EntityEventStream>()
                                  .Push({input::EntityEventType::Create, e});
                          }>();
    world.on_destroy<ReplicatedTag>()
        .template connect<+[](entt::registry& world, entt::entity e)
                          {
                              LOG_DEBUG("destroy entity {}", (uint64_t)e);
                              world.ctx()
                                  .template get<input::EntityEventStream>()
                                  .Push({input::EntityEventType::Destroy, e});
                          }>();
}

using namespace game;

void EntityReplication::Encode(entt::registry& world, ENetPeer* peer,
                               oge::runtime::NetPacketSender& server,
                               FamilyId family, SendType sendType,
                               uint8_t channel, entt::any& anyState)
{
    auto& state = entt::any_cast<State&>(anyState);

    auto* stream = world.ctx().find<input::EntityEventStream>();
    if (!stream) return;

    if (state.useSnapshot)
    {
        for (auto e : world.view<ReplicatedTag>())
        {
            size_t size = sizeof(FamilyId) + sizeof(input::EntityEventType) +
                          sizeof(entt::entity);

            LOG_DEBUG("send entity {}", (uint32_t)e);
            auto packet = server.StartPacket(size);

            packet.Write(family);
            packet.Write(EntityEventType::Create);
            packet.Write(e);

            server.Send(peer, packet);
        }
        state.useSnapshot = false;
        stream->AdvanceCursor(state.cursor);
    }
    else
    {
        input::EntityEvent delta;

        while (stream->PollOne(state.cursor, delta))
        {
            size_t size = sizeof(FamilyId) + sizeof(input::EntityEventType) +
                          sizeof(entt::entity);

            auto packet = server.StartPacket(size);

            packet.Write(family);
            packet.Write(delta.type);
            packet.Write(delta.entity);

            server.Send(peer, packet, sendType, channel);
        }
    }
}

void EntityReplication::Decode(entt::registry& world, net::Buffer& buffer)
{
    input::EntityEventType type;
    buffer.Read(type);

    entt::entity entity;
    buffer.Read(entity);

    switch (type)
    {
        case input::EntityEventType::Create:
        {
            LOG_DEBUG("recive entity {}", (uint32_t)entity);
            auto e = world.create(entity);
            LOG_DEBUG("created entity {}", (uint32_t)e);
            assert(e == entity);
            break;
        }

        case input::EntityEventType::Destroy:
        {
            if (world.valid(entity)) world.destroy(entity);
            break;
        }
    }
}

constexpr size_t MAX_CHUNK_PER_FRAME = 4;

enum class ChunkPacketType : uint8_t
{
    Load,
    Update,
    Discard,
};

entt::any TerrainReplication::CreateState()
{
    return State{};
}

void TerrainReplication::Encode(entt::registry& world, ENetPeer* peer,
                                oge::runtime::NetPacketSender& server,
                                FamilyId family, SendType sendType,
                                uint8_t channel, entt::any& anyState)
{
    using namespace terrain;
    auto& state = entt::any_cast<State&>(anyState);
    auto& terrain = world.ctx().get<terrain::TerrainView>();

    if (state.needsSnapshot)
    {
        if (!state.snapshotCursor.IsValid())
        {
            terrain.GetEvents().AdvanceCursor(state.chunkEventCursor);
        }
        size_t counter = 0;
        for (ChunkHandle& cursor = state.snapshotCursor;
             auto chunk = terrain.PollChunk(cursor);)
        {
            if (chunk->state != ChunkState::Persistent) continue;
            PaletteCompressedChunk cChunk;
            PaletteCompressedChunk::FromChunkData(*chunk, cChunk);
            // LOG_DEBUG("send hash {}, {}", chunk->Coords, DebugHash(cChunk));
            auto packet =
                server.StartPacket(CHUNK_SIZE_TOTAL + 512 * sizeof(uint32_t));
            packet.Write(family);
            packet.Write(ChunkPacketType::Load);
            packet.Write(chunk->Coords);
            packet.Write(cChunk.palette.size());
            packet.WriteRaw(cChunk.palette.data(),
                            cChunk.palette.size() * sizeof(uint32_t));
            packet.WriteRaw(cChunk.data, CHUNK_SIZE_TOTAL);
            server.Send(peer, packet);

            if (++counter >= MAX_CHUNK_PER_FRAME) return;
        }
        state.needsSnapshot = false;
        return;
    }

    size_t counter = 0;
    ChunkStateUpdateEvent e;
    while (terrain.GetEvents().PollOne(state.chunkEventCursor, e))
    {
        auto chunk = terrain.GetChunk(e.chunk);
        if (!chunk || chunk->state != e.state) continue;
        if (e.state == ChunkState::Persistent)
        {
            PaletteCompressedChunk cChunk;
            PaletteCompressedChunk::FromChunkData(*chunk, cChunk);
            // LOG_DEBUG("send hash {}, {}", chunk->Coords, DebugHash(cChunk));
            auto packet =
                server.StartPacket(CHUNK_SIZE_TOTAL + 512 * sizeof(uint32_t));
            packet.Write(family);
            packet.Write(ChunkPacketType::Update);
            packet.Write(chunk->Coords);
            packet.Write(cChunk.palette.size());
            packet.WriteRaw(cChunk.palette.data(),
                            cChunk.palette.size() * sizeof(uint32_t));
            packet.WriteRaw(cChunk.data, CHUNK_SIZE_TOTAL);
            server.Send(peer, packet);
            if (++counter >= MAX_CHUNK_PER_FRAME) return;
        }
    }
}

void TerrainReplication::Decode(entt::registry& world, net::Buffer& buffer)
{
    using namespace terrain;
    auto& terrain = world.ctx().get<terrain::TerrainView>();

    ChunkPacketType ptype;
    buffer.Read(ptype);
    oge::Point3 coord;
    buffer.Read(coord);
    size_t paletteSize;
    buffer.Read(paletteSize);
    PaletteCompressedChunk chunk;
    chunk.palette.resize(paletteSize);
    buffer.ReadRaw(chunk.palette.data(), paletteSize * sizeof(uint32_t));
    buffer.ReadRaw(chunk.data, CHUNK_SIZE_TOTAL);

    // LOG_DEBUG("recv hash {}, {}", coord, DebugHash(chunk));
    assert(ptype == ChunkPacketType::Load || ptype == ChunkPacketType::Update);
    auto handle = terrain.CreateChunk(coord);
    chunk.ToChunkData(*terrain.GetChunk(handle));
    if (ptype == ChunkPacketType::Update)
        terrain.DowngradeChunk(handle, ChunkState::InvalidLighting);
    terrain.UpgradeChunk(handle, ChunkState::Persistent);
}
