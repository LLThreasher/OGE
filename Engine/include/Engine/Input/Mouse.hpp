#pragma once
#include <cinttypes>

namespace OneGame::Engine
{
	enum class MouseButton : uint32_t
	{
		Left = 0,
		Right,
		Middle,
		Button3,
		Button4,
		Button5,
		Button6,
		Button7
	};

	constexpr std::array<MouseButton, 8> ALL_MOUSE_BUTTONS {
		MouseButton::Left,
		MouseButton::Right,
		MouseButton::Middle,
		MouseButton::Button3,
		MouseButton::Button4,
		MouseButton::Button5,
		MouseButton::Button6,
		MouseButton::Button7
	};
}
