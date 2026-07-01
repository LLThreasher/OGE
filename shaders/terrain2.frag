#version 450

layout(location = 0) in vec2 v_LocalUV;
layout(location = 1) in float v_Light;
layout(location = 2) in vec3 v_Color;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

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
   vec2 atlasUV = v_LocalUV;
   
   vec4 texColor = texture(uTexture, atlasUV);
   
   // ---- Final shading ----
   vec3 finalColor = texColor.rgb * v_Color * v_Light;
   
   FragColor = vec4(finalColor, texColor.a);

   // float d = gl_FragCoord.z;
   // FragColor = vec4(d, d, d, 1.0);
   // FragColor = vec4(vec3(1.0, 0.0, 0.0), 1.0); // Debug: solid red
}

