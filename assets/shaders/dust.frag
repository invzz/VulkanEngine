#version 450

layout(location = 0) in float inAlpha;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec4 outColor;

void main()
{
  // Circular particle
  vec2  coord = gl_PointCoord - vec2(0.5);
  float dist  = length(coord);

  if (dist > 0.5) discard;

  // Soft edge
  float alpha = 1.0 - smoothstep(0.0, 0.5, dist);

  outColor = vec4(inColor, alpha * inAlpha);
}
