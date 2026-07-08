#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform Push
{
    mat2 transform;  // rotation * ndc scale
    vec2 offset;     // offset
} pc;

void main()
{
    vec2 pos = pc.transform * vec2(inPosition) + pc.offset;
    fragColor = inColor;
    gl_Position = vec4(pos, 0.0, 1.0);
}
