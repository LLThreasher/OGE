#pragma once

namespace OneGame::Engine
{
	enum TouchState {
		None = 0,
		Pressed,
		Released,
		Moved,
		Cancelled
	};

	struct TouchPoint
	{
		float x;
		float y;
	};
}
