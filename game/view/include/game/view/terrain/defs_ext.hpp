#pragma once

#include <cinttypes>
#include <cassert>
#include <array>

#include "oge/color.hpp"

namespace game::view::terrain
{
using ColorRGBA8 = oge::ColorRGBA8;

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

static constexpr uint32_t CHUNK_VERTEX_BYTE_SIZE = 64 * 1024;  // 64 kb
static constexpr uint32_t CHUNK_INDEX_BYTE_SIZE = 48 * 1024;   // 48 kb
static constexpr uint32_t CHUNK_STORE_BYTE_SIZE = 25 * 1024;   // 25 kb

static constexpr uint32_t MAX_STAGING_PER_FRAME = 6;
static constexpr uint32_t MAX_VISIBLE_CHUNK_NUM = 160;

struct ActiveChunkTag
{
};

struct ChunkSlot
{
    uint32_t chunkSlot;
    uint32_t indexCount;
};

struct ChunkMesh
{
    int32_t chunkX;
    int32_t chunkY;
    int32_t chunkZ;
    ChunkSlot meshSlot;
};

struct Vertex
{
    uint16_t PackedXYZ;
    uint8_t PackedLightAndAO;  // 2 extra bit here
    uint8_t PackedUV;

    Vertex(int x, int y, int z, uint8_t color, uint8_t light, uint8_t ao, uint8_t u, uint8_t v)
    {
        // if (!(x < 32 && y < 32 && z < 32 && x >= 0 && y >= 0 && z >= 0))
        //{
        //	throw std::runtime_error("wrong");
        // }

        assert(x < 32 && y < 32 && z < 32);
        assert(color < 16);
        assert(light < 16 && ao < 4);
        assert(u < 16 && v < 16);

        PackedXYZ = (uint16_t)((x & 0x1F) | ((y & 0x1F) << 5) | ((z & 0x1F) << 10));

        PackedLightAndAO = (uint8_t)((light & 0xF) | ((ao & 0x3) << 4));

        PackedUV = (uint8_t)((u & 0xF) | ((v & 0xF) << 4));
    }
};

static constexpr uint32_t MAXIMUM_VERTEX_BYTE_SIZE = 16 * 16 * 16 / 2 * 4 * 6 * sizeof(Vertex);   // 192 kb
static constexpr uint32_t MAXIMUM_INDEX_BYTE_SIZE = 16 * 16 * 16 / 2 * 6 * 6 * sizeof(uint16_t);  // 144 kb

struct TexturedQuad
{
    // uint16_t PackedXYZFace;		// xyz = 3*4 & face (0-5) 3 bit =>  1
    // extra bit uint16_t PackedLights;		// light per vertex = 4 * 4 = 16
    // bits uint16_t Color;				// color tint R5G5B5
    // uint8_t TextureSlot;			// texture slot on a 16 * 16
    // atlas uint8_t PackedAO;				// AO for 4 corners = 2
    // * 4 = 8 bits
    uint32_t fst = 0u;
    uint32_t sec = 0u;

    TexturedQuad(uint8_t x, uint8_t y, uint8_t z, uint8_t face, uint8_t textureSlot, ColorRGBA8 color,
                 std::array<uint8_t, 4> lights, std::array<uint8_t, 4> AOs)
        : fst(x & 0xF | (((uint64_t)y & 0xF) << 4) | (((uint64_t)z & 0xF) << 8) | (((uint64_t)face & 0x7) << 12) |
              (((uint64_t)lights[0] & 0xF) << 16) | (((uint64_t)lights[1] & 0xF) << 20) |
              (((uint64_t)lights[2] & 0xF) << 24) | (((uint64_t)lights[3] & 0xF) << 28)),
          sec((((uint64_t)(color.r >> 3) & 0x1F) << 0) | (((uint64_t)(color.g >> 3) & 0x1F) << 5) |
              (((uint64_t)(color.b >> 3) & 0x1F) << 10) | (((uint64_t)textureSlot) << 16) |
              (((uint64_t)AOs[0] & 0x3) << 24) | (((uint64_t)AOs[1] & 0x3) << 26) | (((uint64_t)AOs[2] & 0x3) << 28) |
              (((uint64_t)AOs[3] & 0x3) << 30))
    {
        assert(x < 16 && y < 16 && z < 16);
        assert(face < 6);
        assert(lights[0] < 16 && lights[1] < 16 && lights[2] < 16 && lights[3] < 16);
        assert(AOs[0] < 4 && AOs[1] < 4 && AOs[2] < 4 && AOs[3] < 4);
    }
};

static constexpr uint32_t MAXIMUM_QUAD_BYTE_SIZE = 16 * 16 * 16 * 6 * sizeof(TexturedQuad);  // 98 kb

// V3
// x,y,z 12 bit -> array 1
// index + face -> array 2
// array 1 : int16
// array 2 :
//   index 11 bit
//   face	3 bit
//   texId	8 bit
//   color 10 bit -> 3 bit rgb

// V3.1
// x,y,z   12 bit
// face		3 bit
// texSlot  9 bit
}