#version 450

// vec2 is a built-in type representing a 2D vector
// the in keyword indicates that this variable is an input to the vertex shader
layout(location = 0) in vec3 position;

// color will be passed from the vertex shader to the fragment shader
layout(location = 1) in vec3 color;

// normal vector for lighting calculations
layout(location = 2) in vec3 normal;

// uv coordinates for texture mapping
layout(location = 3) in vec2 uv;

// fragColor is an output variable that will be passed to the fragment shader
layout(location = 0) out vec3 fragColor;

layout(location = 1) out vec3 fragmentWorldPos;

layout(location = 2) out vec3 fragmentNormalWorld;

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

// push constants are a way to pass a small amount of data to shaders
layout(push_constant) uniform push_t
{
  mat4 modelMatrix; // prj * view * model
  mat4 normalMatrix;
}
push;

void main()
{
  // transform vertex position to world space
  vec4 positionWorld = push.modelMatrix * vec4(position, 1.0);
  gl_Position        = ubo.proj * ubo.view * positionWorld;

  // transform normal to world space
  vec3 normalWorldSpace = normalize(mat3(push.normalMatrix) * normal);

  // send color to fragment shader for per fragment lighting
  fragColor           = color;
  fragmentWorldPos    = vec3(positionWorld);
  fragmentNormalWorld = normalWorldSpace;
}