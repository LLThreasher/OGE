#pragma once

#include "Engine/ObjectType.hpp"
#include "Engine/entt.hpp"
#include "ISubsystem.hpp"
#include "NetSerializer.hpp"

namespace OneGame::Engine
{
class GameNetSerializer
{
   public:
    uint32_t Serialize(Net::Buffer& buffer, const entt::registry& world);
    uint32_t PopulateSnapshot(const Net::Buffer& buffer, entt::registry& world);
    void InterpolateSnapshot(uint32_t targetFrame, entt::registry& world);
};

class NetPacketSender
{
};

struct SparseWorldSnapshot
{
};

enum class NetObj
{
    ClientState,
    NetPeer,
};

using ClientStateHandle = ResourceHandle<NetObj::ClientState>;
using PeerHandle = ResourceHandle<NetObj::NetPeer>;

class TimedWorldSnapshot
{
   public:
    TimedWorldSnapshot(uint32_t maxGenerationWindow);
    ClientStateHandle CreateClient(PeerHandle peer);
    void DestroyClient(ClientStateHandle client);
    void SendDeltaSnapshot(entt::registry& world, ClientStateHandle client);
    void CaptureGeneration(entt::registry& world, float dt);
    void Acknowledge(PeerHandle peer, uint32_t generation);
};

class IWorldObserver
{
    virtual void Initialize(entt::registry& world, entt::dispatcher& dispatcher) = 0;
    virtual void Update(entt::registry& world) = 0;
};

class GameServer : public IWorldObserver
{
   public:
    void Initialize(entt::registry& world, entt::dispatcher& dispatcher) override;
    void Update(entt::registry& world) override;
    void HandleNewClient(Net::Buffer& buf, entt::registry& world);
    void HandleInputPacket(Net::Buffer& buf, entt::registry& world);
};

class GameClient final : public IWorldObserver
{
   public:
    void Initialize(entt::registry& world, entt::dispatcher& dispatcher) override;
    void Update(entt::registry& world) override;
    void HandleInitPacket(Net::Buffer& buf, entt::registry& world);
    void HandlePacket(Net::Buffer& buf, entt::registry& world);
};
}  // namespace OneGame::Engine
