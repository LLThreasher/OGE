#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec2 v_LocalUV;
layout(location = 1) out vec3 v_Color;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 uMVP;  // 64 bytes
};

void main()
{
    v_Color = inColor;
    v_LocalUV = inUV;
    gl_Position = uMVP * vec4(inPos, 1.0);
}
