#pragma once
#include "Engine/GameAppState.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math.hpp"

#define DECLARE_SUBSYSTEM(Name, ...)                \
class Subsystem##Name : public ISubsystem     \
{                                              \
public:                                        \
	Subsystem##Name(entt::registry& gameWorld) : ISubsystem(gameWorld) {} \
    void Initialize(SubsystemInitContext& ctx) override; \
    void Update(SubsystemContext& ctx) override; \
    void Present(entt::registry& presentationWorld) override; \
private:									\
	__VA_ARGS__                             \
};

namespace OneGame::Engine::ECS
{
	struct SubsystemInitContext
	{
		AppContext& ctx;
	};

	struct SubsystemContext
	{
		float dt;
		InputSystem& input;
		entt::dispatcher& events;
		const Graphics::IGraphicsBackend& backend;
	};

	class ISubsystem
	{
	public:
		ISubsystem(entt::registry& gameWorld) : m_gameWorld(gameWorld)
		{
		}
		virtual ~ISubsystem() = default;
		virtual void Initialize(SubsystemInitContext& ctx) = 0;
		virtual void Update(SubsystemContext& ctx) = 0;
		virtual void Present(entt::registry& presentationWorld) = 0;
	protected:
		entt::registry& m_gameWorld;
	};

#ifdef PLATFORM_ANDROID
	DECLARE_SUBSYSTEM(Camera,
		int panFingerIndex = -1;
		int moveFingerIndex = -1;
		float moveFingerStartX = 0;
		float moveFingerStartY = 0;
	);
#else
	DECLARE_SUBSYSTEM(Camera);
#endif
	DECLARE_SUBSYSTEM(DebugInfo,
		Graphics::GPUMemoryUsage memUsage;
		Graphics::GPUInfo gpuInfo;
		float currentFPS = 0.f;
		float currentFrameTime = 0.f;
		float accumTime = 0.f;
		uint64_t frameCount = 0;
	);
}
