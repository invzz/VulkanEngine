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

struct DirectionalLight
{
  vec4 direction;
  vec4 color;
};

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
  mat4             projection;
  mat4             view;
  vec4             ambientLightColor;
  vec4             cameraPosition;
  PointLight       pointLights[16];
  DirectionalLight directionalLights[16];
  SpotLight        spotLights[16];
  mat4             lightSpaceMatrices[16];
  vec4             pointLightShadowData[4];
  int              pointLightCount;
  int              directionalLightCount;
  int              spotLightCount;
  int              shadowLightCount;
  int              cubeShadowLightCount;
}
ubo;

void main()
{
  vec3 normalizedNormal = normalize(fragmentNormalWorld);
  vec3 viewDirection    = normalize(ubo.cameraPosition.xyz - fragmentWorldPos);
  vec3 ambientLight     = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w;
  vec3 totalLight       = ambientLight;

  for (int i = 0; i < ubo.pointLightCount; i++)
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

  for (int i = 0; i < ubo.directionalLightCount; i++)
  {
    vec3  directionToLight = normalize(-ubo.directionalLights[i].direction.xyz);
    float diffuseFactor    = max(dot(normalizedNormal, directionToLight), 0.0);
    vec3  lightColor       = ubo.directionalLights[i].color.xyz * ubo.directionalLights[i].color.w;

    // Specular
    vec3  halfVector     = normalize(directionToLight + viewDirection);
    float specularFactor = pow(max(dot(normalizedNormal, halfVector), 0.0), 32.0);

    totalLight += lightColor * (diffuseFactor + specularFactor);
  }

  for (int i = 0; i < ubo.spotLightCount; i++)
  {
    vec3  directionToLight = ubo.spotLights[i].position.xyz - fragmentWorldPos;
    float distance         = length(directionToLight);
    directionToLight       = normalize(directionToLight);

    float theta     = dot(directionToLight, normalize(-ubo.spotLights[i].direction.xyz));
    float epsilon   = ubo.spotLights[i].direction.w - ubo.spotLights[i].outerCutoff;
    float intensity = clamp((theta - ubo.spotLights[i].outerCutoff) / epsilon, 0.0, 1.0);

    if (intensity > 0.0)
    {
      float attenuation =
              1.0 / (ubo.spotLights[i].constantAtten + ubo.spotLights[i].linearAtten * distance + ubo.spotLights[i].quadraticAtten * (distance * distance));

      // Fallback if attenuation params are 0
      if (ubo.spotLights[i].constantAtten == 0.0 && ubo.spotLights[i].linearAtten == 0.0 && ubo.spotLights[i].quadraticAtten == 0.0)
      {
        attenuation = 1.0 / (distance * distance);
      }

      vec3  lightColor    = ubo.spotLights[i].color.xyz * ubo.spotLights[i].color.w;
      float diffuseFactor = max(dot(normalizedNormal, directionToLight), 0.0);

      vec3  halfVector     = normalize(directionToLight + viewDirection);
      float specularFactor = pow(max(dot(normalizedNormal, halfVector), 0.0), 32.0);

      totalLight += lightColor * (diffuseFactor + specularFactor) * attenuation * intensity;
    }
  }

  outColor = vec4(totalLight * fragColor, 1.0);
}