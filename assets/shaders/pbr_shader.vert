#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragmentWorldPos;
layout(location = 2) out vec3 fragmentNormalWorld;
layout(location = 3) out vec2 fragUV;

struct PointLight
{
  vec4 position;
  vec4 color;
};

layout(set = 0, binding = 0) uniform UBO
{
  mat4       proj;
  mat4       view;
  vec4       ambientLightColor;
  vec4       cameraPosition;
  PointLight pointLights[16];
  int        numberOfLights;
}
ubo;

// Push constants: Transform (128 bytes) + Material (32 bytes) = 160 bytes total
// Two ranges will be defined in pipeline: [0-128) and [128-160)
layout(push_constant) uniform PushConstants
{
  mat4 modelMatrix;
  mat4 normalMatrix;
  // Material properties (offset 128)
  vec3  albedo;
  float metallic;
  float roughness;
  float ao;
  float isSelected; // 1.0 if selected, 0.0 otherwise
}
push;

void main()
{
  vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
  gl_Position        = ubo.proj * ubo.view * positionWorld;

  vec3 normalWorldSpace = normalize(mat3(push.normalMatrix) * normal);

  fragColor           = color;
  fragmentWorldPos    = vec3(positionWorld);
  fragmentNormalWorld = normalWorldSpace;
  fragUV              = uv;
}
