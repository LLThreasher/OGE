#pragma once
#include "Engine/GameAppState.hpp"
#include "Engine/Input/InputSystem.hpp"

#define DECLARE_SUBSYSTEM(Name, ...)                \
class Subsystem##Name : public ISubsystem     \
{                                              \
public:                                        \
	Subsystem##Name(entt::registry& gameWorld) : ISubsystem(gameWorld) {} \
    void Initialize(SubsystemInitContext& ctx) override; \
    void Update(SubsystemContext& ctx, float dt) override; \
    void Present(entt::registry& presentationWorld) override; \
private:									\
	__VA_ARGS__                             \	
};

namespace OneGame::Engine::ECS
{
	struct SubsystemInitContext
	{
		const Graphics::IGraphicsBackend* backend;
		entt::dispatcher& events;
	};

	struct SubsystemContext
	{
		const Graphics::IGraphicsBackend* backend;
		const InputSystem& input;
		entt::dispatcher& events;
	};

	class ISubsystem
	{
	public:
		ISubsystem(entt::registry& gameWorld) : m_gameWorld(gameWorld)
		{
		}
		~ISubsystem() = default;
		virtual void Initialize(SubsystemInitContext& ctx) = 0;
		virtual void Update(SubsystemContext& ctx, float dt) = 0;
		virtual void Present(entt::registry& presentationWorld) = 0;
	protected:
		entt::registry& m_gameWorld;
	};

	DECLARE_SUBSYSTEM(Camera);
	DECLARE_SUBSYSTEM(DebugInfo,
		Graphics::GPUMemoryUsage memUsage;
		Graphics::GPUInfo gpuInfo;
		float currentFPS = 0.f;
		float accumTime = 0.f;
		uint64_t frameCount = 0;
	);
}
