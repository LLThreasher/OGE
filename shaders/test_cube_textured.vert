#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 fragColor;

layout(set = 0, binding = 1) uniform UBO
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main()
{
    fragUV = inUV;
    fragColor = inColor;

    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
}
