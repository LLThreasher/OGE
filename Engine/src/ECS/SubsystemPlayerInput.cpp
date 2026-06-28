#include "Engine/ECS/ISubsystem.hpp"
#include "Engine/Math.hpp"
#include "Engine/Graphics/PresentationObjects.hpp"

namespace OneGame::Engine::ECS
{
	struct PlayerInputData
	{
		math::vec2 moveDelta;
		math::vec2 panDelta;
	};

	static math::vec2 ScreenSpaceToSufaceSpace(const UIRect rect, math::vec2 screenPos)
	{
		return screenPos - rect.pos;
	}

	void SubsystemPlayerInput::Initialize(AppContext& ctx, entt::registry& gameWorld)
	{
	}

	void SubsystemPlayerInput::Update(AppContext& ctx, entt::registry& gameWorld, const FrameInputData& f)
	{
		// handle move
		for (auto [entity, drag, data] : gameWorld.view<PlayerMovePanel, const UIDrag, PlayerInputData>().each())
		{
			if (drag.inputIndex == PointerIdx::MOUSE)
				continue;
			auto touchIdx = PointerIdx::TouchIdxFromPtrIdx(drag.inputIndex);
			math::vec2 pos = { f.input.GetTouchX(touchIdx), f.input.GetTouchY(touchIdx) };
			data.moveDelta = pos - drag.dragStartPos;
		}
		// handle pan
		for (auto [entity, drag, data] : gameWorld.view<PlayerViewPanel, const UIDrag, PlayerInputData>().each())
		{
			if (drag.inputIndex == PointerIdx::MOUSE)
				continue;
			auto touchIdx = PointerIdx::TouchIdxFromPtrIdx(drag.inputIndex);
			data.panDelta = { f.input.GetTouchDX(touchIdx), f.input.GetTouchDY(touchIdx) };
		}
	}

	void SubsystemPlayerInput::Present(const entt::registry& gameWorld, PresentationContext& pctx, FrameOutputData& f)
	{
	}
}
