#version 450
layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragmentWorldPos;
layout(location = 2) in vec3 fragmentNormalWorld;

layout(push_constant) uniform push_t
{
  mat4 modelMatrix;
  mat4 normalMatrix;
}
push;

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

void main()
{
  vec3 normalizedNormal = normalize(fragmentNormalWorld);
  vec3 viewDirection    = normalize(ubo.cameraPosition.xyz - fragmentWorldPos);
  vec3 ambientLight     = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
  vec3 totalLight       = ambientLight;

  for (int i = 0; i < ubo.numberOfLights; i++)
  {
    vec3  directionToLight   = ubo.pointLights[i].position.xyz - fragmentWorldPos;
    float distanceSquared    = dot(directionToLight, directionToLight);
    float attenuation        = 1.0 / distanceSquared;
    vec3  normalizedLightDir = directionToLight * inversesqrt(distanceSquared);
    float diffuseFactor      = max(dot(normalizedNormal, normalizedLightDir), 0.0);
    vec3  lightColor         = ubo.pointLights[i].color.xyz * ubo.pointLights[i].color.w;

    // Specular reflection (Blinn-Phong)
    vec3  halfVector     = normalize(normalizedLightDir + viewDirection);
    float specularFactor = pow(max(dot(normalizedNormal, halfVector), 0.0), 32.0);

    totalLight += lightColor * (attenuation * (diffuseFactor + specularFactor));
  }

  outColor = vec4(totalLight * fragColor, 1.0);
}