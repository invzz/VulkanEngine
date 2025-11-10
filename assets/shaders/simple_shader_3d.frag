#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

// push constants are a way to pass a small amount of data to shaders
layout(push_constant) uniform push_t
{
  mat4 modelMatrix; // prj * view * model
  mat4 normalMatrix;
}
push;

void main()
{
  outColor = vec4(fragColor, 1.0) * vec4(1.0, 0.0, 0.0, 1.0);
}