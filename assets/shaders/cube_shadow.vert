#version 450

// Only use position for shadow pass - other attributes are bound but unused
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(push_constant) uniform Push
{
  mat4 modelMatrix;
  mat4 lightSpaceMatrix;
  vec4 lightPosAndFarPlane; // xyz = light position, w = far plane
}
push;

layout(location = 0) out vec3 fragWorldPos;

void main()
{
  vec4 worldPos = push.modelMatrix * vec4(inPosition, 1.0);
  fragWorldPos  = worldPos.xyz;

  gl_Position = push.lightSpaceMatrix * worldPos;
}
