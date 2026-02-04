#ifndef SHADOWS_GLSL
#define SHADOWS_GLSL

// ============================================================================
// SHADOW HELPERS
// ============================================================================
//
// CONTRACT:
//   - Including shader must define: ubo, shadowMaps, cubeShadowMaps
//   - ubo.view uses standard GLM convention: -Z forward, +Y up
//   - ubo.directionalCascadeSplits contains POSITIVE distances from camera
//   - Cascade 0 = nearest, Cascade N-1 = farthest
//
// COORDINATE CONVENTION:
//   View-space Z is NEGATED internally to produce positive "depth from camera"
//   This matches the positive split values from CPU.
//
// ============================================================================

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

// Bias tuning - separated for independent adjustment
const float CSM_NORMAL_OFFSET_BASE   = 0.06;   // Base normal offset in world units (further increased)
const float CSM_NORMAL_OFFSET_GROWTH = 0.6;    // Sub-linear growth per cascade
const float CSM_DEPTH_BIAS_BASE      = 0.0025; // Base constant depth bias (slightly increased)
const float CSM_DEPTH_BIAS_SLOPE     = 0.01;   // Slope-scaled depth bias (much higher)
const float CSM_DEPTH_BIAS_GROWTH    = 0.3;    // Sub-linear growth per cascade
const float CSM_DEPTH_BIAS_MIN       = 0.002;  // Minimum bias clamp

// Grazing angle handling
const float CSM_GRAZING_THRESHOLD  = 0.15; // NdotL below this = unreliable (increased)
const float CSM_GRAZING_FADE_START = 0.0;
const float CSM_GRAZING_FADE_END   = 0.2; // Wider fade zone (increased)

// PCF configuration
const float CSM_PCF_RADIUS = 1.5; // Texel radius for PCF sampling

// ============================================================================
// DEBUG MODES (set via ubo.debugMode or shader define)
// ============================================================================

#ifndef CSM_DEBUG_MODE
#define CSM_DEBUG_MODE 0
#endif

// Debug mode values:
// 0 = Off
// 1 = Cascade colors
// 2 = Blend zones
// 3 = Depth precision heatmap
// 4 = Bias visualization

// Debug cascade colors (ROYGBIV-ish for up to 4 cascades)
const vec3 CASCADE_DEBUG_COLORS[4] = vec3[](vec3(0.2, 0.8, 0.2), // Cascade 0: Green (near)
                                            vec3(0.8, 0.8, 0.2), // Cascade 1: Yellow
                                            vec3(0.8, 0.4, 0.2), // Cascade 2: Orange
                                            vec3(0.8, 0.2, 0.2)  // Cascade 3: Red (far)
);

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// Convert world position to positive view-space depth (distance from camera)
// CONTRACT: Returns positive value where larger = farther from camera
// NOTE: This engine uses +Z forward convention (not standard GL -Z forward)
float getViewDepth(vec3 worldPos)
{
  // Engine uses +Z forward, so view-space Z is already positive for objects in front
  return (ubo.view * vec4(worldPos, 1.0)).z;
}

// Get cascade split distance for a given cascade index
// Returns the FAR boundary of the cascade (positive distance from camera)
float getCascadeSplitFar(int cascade)
{
  if (cascade == 0) return ubo.directionalCascadeSplits.x;
  if (cascade == 1) return ubo.directionalCascadeSplits.y;
  if (cascade == 2) return ubo.directionalCascadeSplits.z;
  return ubo.directionalCascadeSplits.w;
}

// Get the NEAR boundary of a cascade (previous split or 0 for cascade 0)
float getCascadeSplitNear(int cascade)
{
  if (cascade <= 0) return 0.0;
  return getCascadeSplitFar(cascade - 1);
}

// Calculate sub-linear cascade scale factor
// Uses sqrt to prevent exponential bias growth in far cascades
float getCascadeScale(int cascadeIndex)
{
  return 1.0 + sqrt(float(cascadeIndex));
}

// ============================================================================
// CASCADE SELECTION
// ============================================================================

// Select cascade based on view-space depth
// Returns cascade index [0, cascadeCount-1]
int selectCascade(float viewDepth)
{
  int cascadeCount = ubo.directionalCascadeCount;

  // Binary-style selection using split boundaries
  // viewDepth is positive, splits are positive, depth increases with distance
  int cascade = 0;
  if (viewDepth > ubo.directionalCascadeSplits.x) cascade = 1;
  if (viewDepth > ubo.directionalCascadeSplits.y && cascadeCount > 2) cascade = 2;
  if (viewDepth > ubo.directionalCascadeSplits.z && cascadeCount > 3) cascade = 3;

  return clamp(cascade, 0, cascadeCount - 1);
}

// Calculate blend factor for cascade transition
// Returns: 0.0 = fully in primary cascade, 1.0 = fully in next cascade
// blendCascade output: the cascade to blend toward (-1 if no blend)
float calculateCascadeBlendFactor(float viewDepth, int primaryCascade, out int blendCascade)
{
  blendCascade = -1;

  float blendWidth = ubo.cascadeBlendWidth;
  if (blendWidth < 0.001) return 0.0;
  if (primaryCascade >= ubo.directionalCascadeCount - 1) return 0.0;

  // FIXED: Blend zone is relative to CASCADE extent, not absolute from zero
  float cascadeNear   = getCascadeSplitNear(primaryCascade);
  float cascadeFar    = getCascadeSplitFar(primaryCascade);
  float cascadeExtent = cascadeFar - cascadeNear;

  // Blend region is at the FAR end of the cascade
  float blendStart = cascadeFar - cascadeExtent * blendWidth;

  if (viewDepth > blendStart)
  {
    blendCascade = primaryCascade + 1;
    return smoothstep(blendStart, cascadeFar, viewDepth);
  }

  return 0.0;
}

// ============================================================================
// SHADOW MAP SAMPLING
// ============================================================================

// Sample a single shadow map with proper bias handling
// cascadeIndex is used ONLY for bias scaling, not for indexing
float sampleShadowMapWithBias(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex, int cascadeIndex, float grazingFade)
{
  if (lightIndex >= ubo.shadowLightCount) return 1.0;

  vec3  N     = normalize(normal);
  vec3  L     = normalize(lightDir);
  float NdotL = max(dot(N, L), 0.0);

  // === NORMAL OFFSET (world space, before light-space transform) ===
  // Sub-linear cascade scaling prevents far cascade float
  float cascadeScale = getCascadeScale(cascadeIndex);
  float sinAngle     = sqrt(1.0 - NdotL * NdotL);

  // Normal offset: scales with angle and cascade, but sub-linearly
  float normalOffset = CSM_NORMAL_OFFSET_BASE * (1.0 + CSM_NORMAL_OFFSET_GROWTH * (cascadeScale - 1.0)) * sinAngle;
  vec3  offsetPos    = worldPos + N * normalOffset;

  // === LIGHT SPACE TRANSFORM ===
  vec4 lightSpacePos = ubo.lightSpaceMatrices[lightIndex] * vec4(offsetPos, 1.0);
  vec3 projCoords    = lightSpacePos.xyz / lightSpacePos.w;
  projCoords.xy      = projCoords.xy * 0.5 + 0.5;

  // Out of bounds = fully lit
  if (any(lessThan(projCoords, vec3(0.0))) || any(greaterThan(projCoords, vec3(1.0))))
  {
    return 1.0;
  }

  // === DEPTH BIAS (in light clip space) ===
  // Separated constant and slope components
  float biasScale = 1.0 + CSM_DEPTH_BIAS_GROWTH * (cascadeScale - 1.0);
  float constBias = CSM_DEPTH_BIAS_BASE * biasScale;
  float slopeBias = CSM_DEPTH_BIAS_SLOPE * biasScale / max(NdotL, CSM_GRAZING_THRESHOLD);
  float totalBias = max(CSM_DEPTH_BIAS_MIN, constBias + slopeBias);

  // === PCF SAMPLING ===
  vec2 texelSize = 1.0 / vec2(textureSize(shadowMaps[lightIndex], 0));

  // Poisson disk for better sampling distribution
  const vec2 poissonDisk[9] = vec2[](vec2(0.0, 0.0),
                                     vec2(-0.94201624, -0.39906216),
                                     vec2(0.94558609, -0.76890725),
                                     vec2(-0.09418410, -0.92938870),
                                     vec2(0.34495938, 0.29387760),
                                     vec2(-0.91588581, 0.45771432),
                                     vec2(-0.81544232, -0.87912464),
                                     vec2(0.97484398, 0.75648379),
                                     vec2(0.44323325, -0.97511554));

  float shadow       = 0.0;
  float compareDepth = projCoords.z - totalBias;

  for (int i = 0; i < 9; i++)
  {
    vec2 uv = projCoords.xy + poissonDisk[i] * texelSize * CSM_PCF_RADIUS;
    shadow += texture(shadowMaps[lightIndex], vec3(uv, compareDepth));
  }
  shadow /= 9.0;

  // Apply grazing fade - surfaces at extreme angles fade to lit
  return mix(1.0, shadow, grazingFade);
}

// ============================================================================
// MAIN CSM FUNCTION
// ============================================================================

// Calculate directional light shadow with CSM and cascade blending
float calculateDirectionalCSMShadow(vec3 worldPos, vec3 normal, vec3 lightDir)
{
  if (ubo.directionalCascadeCount <= 0) return 1.0;

  // Pre-compute common values
  vec3  N     = normalize(normal);
  vec3  L     = normalize(lightDir);
  float NdotL = max(dot(N, L), 0.0);

  // Grazing angle fade - computed once, used for all cascade samples
  float grazingFade = smoothstep(CSM_GRAZING_FADE_START, CSM_GRAZING_FADE_END, NdotL);
  if (grazingFade < 0.01) return 1.0; // Early out for extreme grazing

  // Get positive view-space depth
  float viewDepth = getViewDepth(worldPos);

  // Select primary cascade
  int primaryCascade    = selectCascade(viewDepth);
  int primaryLightIndex = ubo.directionalCascadeBaseIndex + primaryCascade;

  // Sample primary cascade
  float primaryShadow = sampleShadowMapWithBias(worldPos, normal, lightDir, primaryLightIndex, primaryCascade, grazingFade);

  // Check for cascade blending
  int   blendCascade;
  float blendFactor = calculateCascadeBlendFactor(viewDepth, primaryCascade, blendCascade);

  if (blendFactor > 0.001 && blendCascade >= 0)
  {
    int   blendLightIndex = ubo.directionalCascadeBaseIndex + blendCascade;
    float blendShadow     = sampleShadowMapWithBias(worldPos, normal, lightDir, blendLightIndex, blendCascade, grazingFade);

    return mix(primaryShadow, blendShadow, blendFactor);
  }

  return primaryShadow;
}

// ============================================================================
// DEBUG FUNCTIONS
// ============================================================================

// Get cascade index for debug visualization
int getCSMCascadeIndex(vec3 worldPos)
{
  if (ubo.directionalCascadeCount <= 0) return -1;
  return selectCascade(getViewDepth(worldPos));
}

// Get view-space depth for debugging (positive = farther from camera)
float getCSMViewDepth(vec3 worldPos)
{
  return getViewDepth(worldPos);
}

// Get cascade debug color
vec3 getCSMCascadeDebugColor(vec3 worldPos)
{
  int cascade = getCSMCascadeIndex(worldPos);
  if (cascade < 0) return vec3(1.0);
  return CASCADE_DEBUG_COLORS[clamp(cascade, 0, 3)];
}

// Get blend zone visualization (white = in blend zone)
float getCSMBlendZoneDebug(vec3 worldPos)
{
  if (ubo.directionalCascadeCount <= 0) return 0.0;

  float viewDepth = getViewDepth(worldPos);
  int   cascade   = selectCascade(viewDepth);
  int   blendCascade;

  return calculateCascadeBlendFactor(viewDepth, cascade, blendCascade);
}

// Get depth precision heatmap (red = low precision, green = high)
vec3 getCSMDepthPrecisionDebug(vec3 worldPos)
{
  if (ubo.directionalCascadeCount <= 0) return vec3(0.5);

  float viewDepth = getViewDepth(worldPos);
  int   cascade   = selectCascade(viewDepth);

  // Normalized position within cascade [0,1]
  float cascadeNear = getCascadeSplitNear(cascade);
  float cascadeFar  = getCascadeSplitFar(cascade);
  float t           = (viewDepth - cascadeNear) / max(cascadeFar - cascadeNear, 0.001);

  // Green at near (high precision), red at far (low precision)
  return mix(vec3(0.2, 0.8, 0.2), vec3(0.8, 0.2, 0.2), t);
}

// Combined debug overlay - multiply with shadow result
vec3 applyCSMDebugOverlay(vec3 worldPos, float shadow, int debugMode)
{
  if (debugMode == 1)
  {
    // Cascade colors
    return getCSMCascadeDebugColor(worldPos) * shadow;
  }
  else if (debugMode == 2)
  {
    // Blend zones
    float blend = getCSMBlendZoneDebug(worldPos);
    vec3  color = mix(getCSMCascadeDebugColor(worldPos), vec3(1.0), blend * 0.5);
    return color * shadow;
  }
  else if (debugMode == 3)
  {
    // Depth precision
    return getCSMDepthPrecisionDebug(worldPos) * shadow;
  }

  return vec3(shadow);
}

// ============================================================================
// SPOT LIGHT SHADOWS (unchanged logic, cleaner structure)
// ============================================================================

// Legacy/spot light shadow sampling (no cascade scaling)
float sampleShadowMap(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex)
{
  return sampleShadowMapWithBias(worldPos, normal, lightDir, lightIndex, 0, 1.0);
}

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

  if (any(lessThan(projCoords, vec3(0.0))) || any(greaterThan(projCoords, vec3(1.0))))
  {
    return 1.0;
  }

  vec2  texelSize    = 1.0 / vec2(textureSize(shadowMaps[lightIndex], 0));
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
