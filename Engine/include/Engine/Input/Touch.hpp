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
		int id;
		float x;
		float y;
		TouchState event;
	};
}
