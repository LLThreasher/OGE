#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform Push
{
    vec2 ndcScale;   // = 2.0 / screenSize
} pc;

void main()
{
    vec2 pos = inPosition.xy * pc.ndcScale - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);
}
