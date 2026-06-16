#pragma once
#include <cassert>

namespace OneGame::Engine::Terrain
{
	static constexpr char FACE_FRONT = 5;
	static constexpr char FACE_BACK = 6;
	static constexpr char FACE_TOP = 2;
	static constexpr char FACE_DOWN = 3;
	static constexpr char FACE_LEFT = 0;
	static constexpr char FACE_RIGHT = 1;
	
	static constexpr char WHITE = 0;
	static constexpr char RED = 1;
	static constexpr char GREEN = 2;
	static constexpr char BLUE = 3;
	static constexpr char YELLOW = 4;
	static constexpr char CYAN = 5;
	static constexpr char MAGENTA = 6;

	struct Vertex
	{

		uint16_t PackedXYZColor;
		uint8_t PackedLightAndAO; // 2 extra bit here
		uint8_t PackedUV;

		Vertex(uint8_t x, uint8_t y, uint8_t z, uint8_t color, uint8_t light, uint8_t ao, uint8_t u, uint8_t v)
		{
			assert(x < 16 && y < 16 && z < 16);
			assert(color < 16);
			assert(light < 16 && ao < 4);
			assert(u < 16 && v < 16);

			PackedXYZColor = (uint16_t)(
				(x & 0xF) |
				((y & 0xF) << 4) |
				((z & 0xF) << 8) |
				((color & 0xF) << 12)
				);

			PackedLightAndAO =
				(uint8_t)((light & 0xF) |
					((ao & 0x3) << 4));

			PackedUV = (uint8_t)((u & 0xF) | ((v & 0xF) << 4));
		}
	};
}
