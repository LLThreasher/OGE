#version 450
// General 3D mesh vertex shader — supports vertex color, UV texturing, normals.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_UV;
layout(location = 3) out vec4 v_Color;

layout(set = 0, binding = 0) uniform UBO
{
    mat4 uModel;
    mat4 uMVP;      // combined model-view-projection
    mat4 uNormalMatrix; // transpose(inverse(model))
};

void main()
{
    vec4 worldPos = uModel * vec4(inPosition, 1.0);
    v_WorldPos = worldPos.xyz;
    v_Normal   = mat3(uNormalMatrix) * inNormal;
    v_UV       = inUV;
    v_Color    = inColor;
    gl_Position = uMVP * vec4(inPosition, 1.0);
}
