#version 450

layout(location = 0) in uvec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4  inColor;
layout(location = 3) in uint  inTexIndex;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;
layout(location = 2) flat out uint fragTexIndex;

layout(push_constant) uniform Push
{
    mat2 transform;  // rotation * ndc scale
    vec2 offset;     // offset
} pc;

const float INV_U8  = 1.0 / 255.0;

void main()
{
    vec2 pos = pc.transform * inPosition.xy + pc.offset;

    fragUV = vec2(inUV);

    fragColor = inColor * INV_U8;

    fragTexIndex = inTexIndex;

    gl_Position = vec4(pos, 0.0, 1.0);
}