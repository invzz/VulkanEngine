#version 450

layout(location = 0) out vec3 fragColor;

struct DirectionalLight
{
  vec4 direction;
  vec4 color;
};

layout(set = 0, binding = 0) uniform UBO
{
  mat4             proj;
  mat4             view;
  vec4             ambientLightColor;
  vec4             cameraPosition;
  vec4             pointLights[16 * 2]; // Placeholder
  DirectionalLight directionalLights[16];
  // ... rest of UBO
}
ubo;

layout(push_constant) uniform push_t
{
  mat4 modelMatrix;
  vec4 color;
}
push;

vec3 getArrowVertex(int index)
{
  // Main shaft (line)
  if (index == 0) return vec3(0.0, 0.0, 0.0);
  if (index == 1) return vec3(0.0, 0.0, -2.0);
  if (index == 2) return vec3(0.0, 0.0, -2.0);
  if (index == 3) return vec3(0.0, 0.0, -2.0);

  // Arrow head - triangle pointing down
  if (index == 4) return vec3(0.0, 0.0, -2.0);
  if (index == 5) return vec3(-0.3, 0.0, -1.6);
  if (index == 6) return vec3(0.0, 0.0, -2.0);
  if (index == 7) return vec3(0.3, 0.0, -1.6);
  if (index == 8) return vec3(0.0, 0.0, -2.0);
  if (index == 9) return vec3(0.0, -0.3, -1.6);
  if (index == 10) return vec3(0.0, 0.0, -2.0);
  if (index == 11) return vec3(0.0, 0.3, -1.6);

  // Cross bars for visibility
  if (index == 12) return vec3(-0.3, 0.0, -1.6);
  if (index == 13) return vec3(0.3, 0.0, -1.6);
  if (index == 14) return vec3(0.0, -0.3, -1.6);
  if (index == 15) return vec3(0.0, 0.3, -1.6);

  // Additional cross at origin for reference
  if (index == 16) return vec3(-0.15, 0.0, 0.0);
  if (index == 17) return vec3(0.15, 0.0, 0.0);

  return vec3(0.0);
}

void main()
{
  vec3 vertexPos = getArrowVertex(gl_VertexIndex);
  vec4 worldPos  = push.modelMatrix * vec4(vertexPos, 1.0);
  gl_Position    = ubo.proj * ubo.view * worldPos;
  fragColor      = push.color.rgb;
}
