#version 450

layout(location = 0) in uvec2 inPosition;
layout(location = 1) in vec2  inUV;
layout(location = 2) in vec4  inColor;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;

layout(push_constant) uniform Push
{
    mat2 transform;  // rotation * ndc scale
    vec2 offset;     // offset
} pc;

void main()
{
    vec2 pos = pc.transform * inPosition.xy + pc.offset;

    fragUV = inUV;

    fragColor = inColor;

    gl_Position = vec4(pos, 0.0, 1.0);
}