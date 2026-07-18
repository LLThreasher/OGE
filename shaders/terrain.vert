#version 450

layout (location = 0) in uint aPackedPosition;
layout (location = 1) in uint aPackedLightAndAO;
layout (location = 2) in uint aPackedUV;

layout(location = 0) out vec2 v_LocalUV;
layout(location = 1) out float v_Light;
layout(location = 2) flat out uint v_ColorIndex;

layout(set = 0, binding = 2) uniform ObjectBlock
{
    mat4 uMVP;  // 64 bytes
};

void main()
{
    uint x =  aPackedPosition        & 31u;
    uint y = (aPackedPosition >> 5u) & 31u;
    uint z = (aPackedPosition >> 10u) & 31u;
    // uint aColor = (aPackedPosition >> 12u) & 15u;
    uint aColor = 0;

    vec3 aPosition = vec3(x, y, z);

    gl_Position = uMVP * vec4(aPosition, 1.0);

    float u = float(aPackedUV & 15u) / 15.0;
    float v = float((aPackedUV >> 4) & 15u) / 15.0;
    v_LocalUV = vec2(u, v);
    v_Light = float(aPackedLightAndAO & 15u) / 15.0;

    v_ColorIndex = aColor;
}
