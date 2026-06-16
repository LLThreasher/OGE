#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) flat in int fragTexIndex;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uTextures[16];

void main()
{
    vec4 texColor = texture(uTextures[fragTexIndex], fragUV);

    // If it's a font atlas (R8), only .r is valid
    // For RGBA textures, this works normally
    float alpha = texColor.a > 0.0 ? texColor.a : texColor.r;

    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}
