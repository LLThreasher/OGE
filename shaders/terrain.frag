#version 450

layout(location = 0) in vec2 v_LocalUV;
layout(location = 1) in float v_Light;
layout(location = 2) flat in uint v_ColorIndex;
layout(location = 3) flat in uint v_Material;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(push_constant) uniform Push
{
    uint uColorPalette[16];
} pc;

layout(location = 0) out vec4 FragColor;

vec4 unpackColor(uint packed)
{
    float r = float((packed >>  0) & 0xFFu) / 255.0;
    float g = float((packed >>  8) & 0xFFu) / 255.0;
    float b = float((packed >> 16) & 0xFFu) / 255.0;
    float a = float((packed >> 24) & 0xFFu) / 255.0;

    return vec4(r, g, b, a);
}

void main()
{
   // ---- Atlas lookup ----
   uint tileX = v_Material & 15u;          // % 16
   uint tileY = v_Material >> 4u;          // / 16
   
   vec2 tileSize = vec2(1.0 / 16.0);
   
   vec2 atlasUV =
       vec2(float(tileX), float(tileY)) * tileSize
       + v_LocalUV * tileSize;
   
   vec4 texColor = texture(uTexture, atlasUV);
   
   // ---- Color palette lookup ----
   vec3 tint = unpackColor(pc.uColorPalette[v_ColorIndex]).rgb;
   
   // ---- Final shading ----
   vec3 finalColor = texColor.rgb * tint * v_Light;
   
   FragColor = vec4(finalColor, texColor.a);
   // FragColor = vec4(tint, 1); // Debug: solid red
}

