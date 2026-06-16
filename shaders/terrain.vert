#version 450

layout (location = 0) in uint aPackedPosition;
layout (location = 1) in uint aColor;
layout (location = 2) in uint aMaterialIdx;

layout(location = 0) out vec2 v_LocalUV;
layout(location = 1) out float v_Light;
layout(location = 2) flat out uint v_ColorIndex;
layout(location = 3) flat out uint v_Material;

const vec2 uvTable[4] = vec2[](
    vec2(0.0, 0.0), // corner 0
    vec2(1.0, 0.0), // corner 1
    vec2(1.0, 1.0), // corner 2
    vec2(0.0, 1.0) // corner 3
);

layout(set = 0, binding = 1) uniform ObjectBlock
{
    mat4 uMVP;  // 64 bytes
};

void main()
{
    uint x =  aPackedPosition        & 15u;
    uint y = (aPackedPosition >> 4u) & 15u;
    uint z = (aPackedPosition >> 8u)& 15u;
    uint aCorner = (aPackedPosition >> 12u) & 3u; // Assuming corner is packed in bits 12-13

    vec3 aPosition = vec3(x, y, z);

    gl_Position = uMVP * vec4(aPosition, 1.0);

    v_LocalUV = uvTable[aCorner];
    v_Light = float(aColor >> 4) / 15.0;

    v_ColorIndex = aColor & 15u;
    v_Material = aMaterialIdx;
}
