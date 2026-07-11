#include "Engine/ECS/GameNetSerializer.hpp"

namespace OneGame::Engine
{
void GameClient::Initialize(entt::registry& world, entt::dispatcher& dispatcher) {}

void GameClient::HandleInitPacket(Net::Buffer& buf, entt::registry& world) {}

void GameClient::HandlePacket(Net::Buffer& buf, entt::registry& world) {}

void GameClient::Update(entt::registry& world) {}

void GameServer::Initialize(entt::registry& world, entt::dispatcher& dispatcher) {}

void GameServer::Update(entt::registry& world) {}

void GameServer::HandleNewClient(Net::Buffer& buf, entt::registry& world) {}

void GameServer::HandleInputPacket(Net::Buffer& buf, entt::registry& world) {}

}  // namespace OneGame::Engine