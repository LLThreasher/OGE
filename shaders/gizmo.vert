#version 450
// Gizmo vertex shader — wireframe rendering with solid color.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 v_Color;

layout(push_constant) uniform PushConstants
{
    mat4 uMVP;
};

void main()
{
    v_Color    = inColor;
    gl_Position = uMVP * vec4(inPosition, 1.0);
}
