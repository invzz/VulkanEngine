#version 450

layout(location = 0) in vec2 fragOffset;
layout(location = 0) out vec4 outColor;
struct PointLight
{
  vec4 position; // w is ignored
  vec4 color;    // w component can be used for intensity
};

layout(set = 0, binding = 0) uniform UBO
{
  mat4       proj;
  mat4       view;
  vec4       ambientLightColor;
  PointLight pointLights[16];
  int        numberOfLights;
}
ubo;

layout(push_constant) uniform push_t
{
  vec4  lightPosition;
  vec4  color;
  float radius;
}
push;

void main()
{
  float distance = length(fragOffset);
  if (distance >= 1.0) discard;
  outColor = push.color;
}