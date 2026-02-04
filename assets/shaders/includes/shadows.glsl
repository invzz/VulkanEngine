#ifndef SHADOWS_GLSL
#define SHADOWS_GLSL

// Shadow helpers.
// Note: Expects the including shader to define `ubo`, `shadowMaps`, and `cubeShadowMaps`.

// ============================================================================
// SIMPLE CSM IMPLEMENTATION - NO BLENDING
// ============================================================================

// Calculate shadow for a single cascade/light
float sampleShadowMap(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex)
{
  if (lightIndex >= ubo.shadowLightCount) return 1.0;

  vec4 lightSpacePos = ubo.lightSpaceMatrices[lightIndex] * vec4(worldPos, 1.0);
  vec3 projCoords    = lightSpacePos.xyz / lightSpacePos.w;
  projCoords.xy      = projCoords.xy * 0.5 + 0.5;

  // Out of bounds check
  if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z < 0.0 || projCoords.z > 1.0)
  {
    return 1.0; // Outside shadow map = lit
  }

  vec2 texelSize = 1.0 / textureSize(shadowMaps[lightIndex], 0);

  // Simple bias
  float NdotL = max(dot(normalize(normal), normalize(lightDir)), 0.0);
  float bias  = max(0.001, 0.005 * (1.0 - NdotL));

  // Simple 3x3 PCF
  float shadow = 0.0;
  for (int x = -1; x <= 1; x++)
  {
    for (int y = -1; y <= 1; y++)
    {
      vec2  uv           = projCoords.xy + vec2(x, y) * texelSize;
      float compareDepth = projCoords.z - bias;
      shadow += texture(shadowMaps[lightIndex], vec3(uv, compareDepth));
    }
  }
  return shadow / 9.0;
}

// Main CSM function - SIMPLE VERSION (no blending)
float calculateDirectionalCSMShadow(vec3 worldPos, vec3 normal, vec3 lightDir)
{
  if (ubo.directionalCascadeCount <= 0) return 1.0;

  // Get view-space Z (positive = in front of camera for +Z forward)
  float viewZ = (ubo.view * vec4(worldPos, 1.0)).z;

  // Get cascade splits
  vec4 splits = ubo.directionalCascadeSplits;

  // Select cascade based on view Z
  int cascade = 0;
  if (viewZ > splits.x) cascade = 1;
  if (viewZ > splits.y && ubo.directionalCascadeCount > 2) cascade = 2;
  if (viewZ > splits.z && ubo.directionalCascadeCount > 3) cascade = 3;
  cascade = clamp(cascade, 0, ubo.directionalCascadeCount - 1);

  // Sample the selected cascade
  int lightIndex = ubo.directionalCascadeBaseIndex + cascade;
  return sampleShadowMap(worldPos, normal, lightDir, lightIndex);
}

// Returns the cascade index for debugging purposes
int getCSMCascadeIndex(vec3 worldPos)
{
  if (ubo.directionalCascadeCount <= 0) return -1;

  float viewZ  = (ubo.view * vec4(worldPos, 1.0)).z;
  vec4  splits = ubo.directionalCascadeSplits;

  int cascade = 0;
  if (viewZ > splits.x) cascade = 1;
  if (viewZ > splits.y && ubo.directionalCascadeCount > 2) cascade = 2;
  if (viewZ > splits.z && ubo.directionalCascadeCount > 3) cascade = 3;

  return clamp(cascade, 0, ubo.directionalCascadeCount - 1);
}

// Returns view-space depth for debugging
float getCSMViewDepth(vec3 worldPos)
{
  return (ubo.view * vec4(worldPos, 1.0)).z;
}

// ============================================================================
// SPOT LIGHT SHADOWS
// ============================================================================

float calculateShadow(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex)
{
  return sampleShadowMap(worldPos, normal, lightDir, lightIndex);
}

// Backwards-compatible wrapper (no normal/lightDir available)
float calculateShadow(vec3 worldPos, int lightIndex)
{
  if (lightIndex >= ubo.shadowLightCount) return 1.0;

  vec4 lightSpacePos = ubo.lightSpaceMatrices[lightIndex] * vec4(worldPos, 1.0);
  vec3 projCoords    = lightSpacePos.xyz / lightSpacePos.w;
  projCoords.xy      = projCoords.xy * 0.5 + 0.5;

  if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z < 0.0 || projCoords.z > 1.0)
  {
    return 1.0;
  }

  vec2  texelSize    = 1.0 / textureSize(shadowMaps[lightIndex], 0);
  float compareDepth = projCoords.z - 0.001;

  float shadow = 0.0;
  for (int x = -1; x <= 1; x++)
  {
    for (int y = -1; y <= 1; y++)
    {
      vec2 offset = vec2(x, y) * texelSize;
      shadow += texture(shadowMaps[lightIndex], vec3(projCoords.xy + offset, compareDepth));
    }
  }
  return shadow / 9.0;
}

// ============================================================================
// POINT LIGHT SHADOWS (CUBE MAP)
// ============================================================================

float calculatePointLightShadow(vec3 worldPos, int lightIndex)
{
  if (lightIndex >= ubo.cubeShadowLightCount) return 1.0;

  vec3  lightPos = ubo.pointLightShadowData[lightIndex].xyz;
  float farPlane = ubo.pointLightShadowData[lightIndex].w;

  vec3  lightToFrag  = worldPos - lightPos;
  float currentDepth = length(lightToFrag);

  if (currentDepth > farPlane) return 1.0;

  // Flip Y for Vulkan cube maps
  vec3 sampleDir = vec3(lightToFrag.x, -lightToFrag.y, lightToFrag.z);

  float closestDepth    = texture(cubeShadowMaps[lightIndex], sampleDir).r;
  float normalizedDepth = currentDepth / farPlane;
  float bias            = 0.02;

  return (normalizedDepth > closestDepth + bias) ? 0.0 : 1.0;
}

#endif // SHADOWS_GLSL
