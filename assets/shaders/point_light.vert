#version 450

const vec2 OFFSETS[6] = vec2[](vec2(-1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));

layout(location = 0) out vec2 fragOffset;

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
  fragOffset                 = OFFSETS[gl_VertexIndex];
  vec4 lightInCameraSpace    = ubo.view * push.lightPosition;
  vec4 positionInCameraSpace = lightInCameraSpace + push.radius * vec4(fragOffset, 0.0, 0.0);
  gl_Position                = ubo.proj * positionInCameraSpace;
}
