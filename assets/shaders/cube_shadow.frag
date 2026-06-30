#version 450

layout(location = 0) in vec3 fragWorldPos;

layout(push_constant) uniform Push
{
  mat4 modelMatrix;
  mat4 lightSpaceMatrix;
  vec4 lightPosAndFarPlane; // xyz = light position, w = far plane
}
push;

void main()
{
  // Calculate distance from fragment to light
  vec3  lightPos           = push.lightPosAndFarPlane.xyz;
  float farPlane           = push.lightPosAndFarPlane.w;
  float distance           = length(fragWorldPos - lightPos);
  float normalizedDistance = distance / farPlane;

  // Write normalized distance as depth
  gl_FragDepth = clamp(normalizedDistance, 0.0, 1.0);
}
