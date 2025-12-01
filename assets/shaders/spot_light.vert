#version 450

layout(location = 0) out vec3 fragColor;

struct SpotLight
{
  vec4  position;
  vec4  direction;
  vec4  color;
  float outerCutoff;
  float constantAtten;
  float linearAtten;
  float quadraticAtten;
};

layout(set = 0, binding = 0) uniform UBO
{
  mat4      proj;
  mat4      view;
  vec4      ambientLightColor;
  vec4      cameraPosition;
  vec4      pointLights[16 * 2];       // Placeholder
  vec4      directionalLights[16 * 2]; // Placeholder
  SpotLight spotLights[16];
  // ... rest of UBO
}
ubo;

layout(push_constant) uniform push_t
{
  mat4  modelMatrix;
  vec4  color;
  float coneAngle; // Outer cutoff angle in radians
}
push;

vec3 getConeVertex(int index, float angle)
{
  // Cone apex at origin
  if (index == 0) return vec3(0.0, 0.0, 0.0);

  // Cone base circle (16 segments) at z = 2.0 (positive Z)
  float radius      = tan(angle) * 2.0;
  int   circleIndex = index - 1;

  if (circleIndex < 16)
  {
    float theta = float(circleIndex) * 2.0 * 3.14159265359 / 16.0;
    return vec3(radius * cos(theta), radius * sin(theta), 2.0);
  }

  // Lines from apex to circle (every other point for clarity)
  int lineIndex = circleIndex - 16;
  if (lineIndex < 8)
  {
    if (lineIndex % 2 == 0)
      return vec3(0.0, 0.0, 0.0); // Apex
    else
    {
      int   circlePoint = (lineIndex / 2) * 2;
      float theta       = float(circlePoint) * 2.0 * 3.14159265359 / 16.0;
      return vec3(radius * cos(theta), radius * sin(theta), 2.0);
    }
  }

  return vec3(0.0);
}

void main()
{
  vec3 vertexPos = getConeVertex(gl_VertexIndex, push.coneAngle);
  vec4 worldPos  = push.modelMatrix * vec4(vertexPos, 1.0);
  gl_Position    = ubo.proj * ubo.view * worldPos;
  fragColor      = push.color.rgb;
}
