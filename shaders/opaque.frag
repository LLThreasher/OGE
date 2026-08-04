#version 450

layout(location = 0) in vec2 v_LocalUV;
layout(location = 1) in vec3 v_Color;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 FragColor;

void main()
{
    vec4 texColor = texture(uTexture, v_LocalUV);
    FragColor = vec4(v_Color * texColor.rgb, 1.0);
}
