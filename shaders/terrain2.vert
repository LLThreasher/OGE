#version 450

struct Vertex
{
    uint fst;
    uint sec;
};

layout(location = 0) out vec2 v_LocalUV;
layout(location = 1) out float v_Light;
layout(location = 2) out vec3 v_Color;
layout(location = 3) flat out uint u_Layer;

layout(set = 0, binding = 2) readonly buffer ObjectBlock
{
    mat4 uMVP;  // 64 bytes
};

layout(set = 0, binding = 1) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

const uint index_to_corner[6] = { 0, 1, 2, 2, 3, 0 };

const vec3 faceTable[24] =
{
    // left:    -x face
    vec3(0, 0, 0),
    vec3(0, 0, 1),
    vec3(0, 1, 1),
    vec3(0, 1, 0),
    // right:   +x face
    vec3(1, 0, 1),
    vec3(1, 0, 0),
    vec3(1, 1, 0),
    vec3(1, 1, 1),
    // up:      +y face
    vec3(0, 1, 1),
    vec3(1, 1, 1),
    vec3(1, 1, 0),
    vec3(0, 1, 0),
    // down:    -y face
    vec3(0, 0, 0),
    vec3(1, 0, 0),
    vec3(1, 0, 1),
    vec3(0, 0, 1),
    // front:   +z face
    vec3(0, 0, 1),
    vec3(1, 0, 1),
    vec3(1, 1, 1),
    vec3(0, 1, 1),
    // back:    -z face
    vec3(1, 0, 0),
    vec3(0, 0, 0),
    vec3(0, 1, 0),
    vec3(1, 1, 0),
};

const vec2 uvTable[4] = vec2[](
    vec2(1.0, 1.0), // corner 0
    vec2(0.0, 1.0), // corner 1
    vec2(0.0, 0.0), // corner 2
    vec2(1.0, 0.0)  // corner 3
);

void main()
{
    Vertex v = vertices[gl_VertexIndex / 6];
    uint cur_idx = index_to_corner[gl_VertexIndex % 6];
    uint x =  v.fst        & 15u;
    uint y = (v.fst >> 4u) & 15u;
    uint z = (v.fst >> 8u) & 15u;
    uint face = (v.fst >> 12u) & 7u;
    vec3 aPosition = vec3(x, y, z) + faceTable[(face << 2u) + cur_idx];
    gl_Position = uMVP * vec4(aPosition, 1.0);

    float light = float((v.fst >> (16u + (cur_idx << 2u))) & 15u) / 15.0;
    float ao = float(4u - ((v.sec >> (24u + (cur_idx << 1u))) & 3u)) / 4.0;
    v_Light = light * ao;
    
    uint color_r = v.sec    & 31u;
    uint color_g = (v.sec >> 5u) & 31u;
    uint color_b = (v.sec >> 10u) & 31u;
    v_Color = vec3(float(color_r) / 31.0, float(color_g) / 31.0, float(color_b) / 31.0);
    
    uint texSlot = (v.sec >> 16u) & 255u;
    u_Layer = texSlot;
    
    const float tileSize = 1.0 / 16.0;
    
    // uint tileX = texSlot & 15u;
    // uint tileY = texSlot >> 4u;
    // 
    // vec2 tileOffset = vec2(tileX, tileY) * tileSize;
    // v_LocalUV = tileOffset + uvTable[cur_idx] * tileSize;
    
    v_LocalUV = uvTable[cur_idx];
}
