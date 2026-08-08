#version 450
// General 3D mesh fragment shader — textured, vertex-colored, with normal-based
// simple directional lighting.

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_UV;
layout(location = 3) in vec4 v_Color;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 0, binding = 1) uniform UBO
{
    vec4 uLightDir;      // world-space directional light (xyz = dir, w unused)
    vec4 uAmbientColor;  // ambient term
    vec4 uLightColor;    // diffuse term
};

void main()
{
    // Sample texture (white if no texture bound — bind a 1x1 white texture).
    vec4 texColor = texture(uTexture, v_UV);

    // Combine texture with vertex color.
    vec4 baseColor = texColor * v_Color;

    // Simple Blinn-Phong diffuse.
    vec3 N = normalize(v_Normal);
    vec3 L = normalize(uLightDir.xyz);
    float NdotL = max(dot(N, L), 0.0);

    vec3 ambient  = uAmbientColor.rgb * baseColor.rgb;
    vec3 diffuse  = uLightColor.rgb * baseColor.rgb * NdotL;

    FragColor = vec4(ambient + diffuse, baseColor.a);
}
