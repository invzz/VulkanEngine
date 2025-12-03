#version 450

// Only use position for shadow pass
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(push_constant) uniform PushConstants
{
  mat4 modelMatrix;
  mat4 lightSpaceMatrix;
}
push;

void main()
{
  vec4 worldPos = push.modelMatrix * vec4(inPosition, 1.0);
  gl_Position   = push.lightSpaceMatrix * worldPos;
}
