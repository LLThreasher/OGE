#pragma once
#include "Engine/GameAppState.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/Graphics/IGraphicsBackend.hpp"

#define DECLARE_SUBSYSTEM(Name, ...)                \
class Subsystem##Name : public ISubsystem     \
{                                              \
public:                                        \
    void Initialize(AppContext& ctx, entt::registry& gameWorld) override; \
    void Update(AppContext& ctx, entt::registry& gameWorld, const FrameInputData& fd) override; \
    void Present(const entt::registry& gameWorld, PresentationContext& ctx, FrameOutputData& fd) override; \
private:									\
	__VA_ARGS__                             \
};

namespace OneGame::Engine::ECS
{
	class ISubsystem
	{
	public:
		virtual ~ISubsystem() = default;
		virtual void Initialize	(AppContext& ctx, entt::registry& gameWorld) = 0;
		virtual void Update		(AppContext& ctx, entt::registry& gameWorld, const FrameInputData& fd) = 0;
		virtual void Present	(const entt::registry& gameWorld, PresentationContext& ctx, FrameOutputData& fd) = 0;
	};

	DECLARE_SUBSYSTEM(Camera);
	DECLARE_SUBSYSTEM(DebugInfo,
		float currentFPS = 0.f;
		float currentFrameTime = 0.f;
		float accumTime = 0.f;
		uint64_t frameCount = 0;
	);

	DECLARE_SUBSYSTEM(PlayerInput);

	struct UIDrag
	{
		int inputIndex = -1;
		MouseButton dragStartButton;
		math::vec2 dragStartPos;
	};

	struct UIDragRelease
	{
		UIDrag drag;
		entt::entity dragStart;
	};

	struct UIRaycastTarget
	{
	};

	struct UIRaycastHit
	{
		entt::entity dragStart;
	};

	struct UIRect
	{
		int zLevel;
		math::vec2 pos;
		math::vec2 extend;
		std::string spriteId;
	};

	struct PlayerMovePanel
	{
	};

	struct PlayerViewPanel
	{
	};

	// Handle drags and UI rendering
	DECLARE_SUBSYSTEM(UI,
		std::array<entt::entity, PointerIdx::COUNT> activeDrags;
		void onCreateUIRect(entt::registry& gameWorld, entt::entity entity);
	);
}
