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

// uniform buffer object for global data
layout(set = 0, binding = 0) uniform GlobalUbo
{
  mat4 projectionViewMatrix;
  vec3 directionToLight;
}
ubo;

// push constants are a way to pass a small amount of data to shaders
layout(push_constant) uniform push_t
{
  mat4 modelMatrix; // prj * view * model
  mat4 normalMatrix;
}
push;

const float AMBIENT_LIGHT_INTENSITY = 0.2;

void main()
{
  gl_Position            = ubo.projectionViewMatrix * push.modelMatrix * vec4(position, 1.0);
  vec3  normalWorldSpace = normalize(mat3(push.normalMatrix) * normal);
  float lightIntensity   = AMBIENT_LIGHT_INTENSITY + max(dot(normalWorldSpace, ubo.directionToLight), 0.0);
  fragColor              = lightIntensity * color;
}