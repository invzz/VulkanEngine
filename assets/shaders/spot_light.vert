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

const float PI       = 3.14159265359;
const int   SEGMENTS = 32;

vec3 getConeVertex(int index, float angle)
{
  int triangleIndex = index / 3;
  int vertexIndex   = index % 3;

  // Apex
  if (vertexIndex == 0)
  {
    return vec3(0.0, 0.0, 0.0);
  }

  // Cone base circle at z = 2.0 (positive Z)
  float length = 2.0;
  float radius = tan(angle) * length;

  // For vertexIndex 1, use triangleIndex
  // For vertexIndex 2, use triangleIndex + 1
  int circleIndex = triangleIndex + (vertexIndex - 1);

  float theta = float(circleIndex) * 2.0 * PI / float(SEGMENTS);
  return vec3(radius * cos(theta), radius * sin(theta), length);
}

void main()
{
  vec3 vertexPos = getConeVertex(gl_VertexIndex, push.coneAngle);
  vec4 worldPos  = push.modelMatrix * vec4(vertexPos, 1.0);
  gl_Position    = ubo.proj * ubo.view * worldPos;
  fragColor      = push.color.rgb;
}
