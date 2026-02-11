#ifndef SHADOWS_GLSL
#define SHADOWS_GLSL

// ============================================================================
// SHADOW HELPERS
// ============================================================================

// Constants (fixed tuning)
const float CSM_NORMAL_OFFSET_BASE   = 0.06;
const float CSM_NORMAL_OFFSET_GROWTH = 0.6;
const float CSM_DEPTH_BIAS_BASE      = 0.0025;
const float CSM_DEPTH_BIAS_SLOPE     = 0.01;
const float CSM_DEPTH_BIAS_GROWTH    = 0.3;
const float CSM_DEPTH_BIAS_MIN       = 0.002;
const float CSM_GRAZING_THRESHOLD    = 0.15;
const float CSM_GRAZING_FADE_START   = 0.0;
const float CSM_GRAZING_FADE_END     = 0.2;
const int   CSM_MAX_POISSON_SAMPLES  = 9;
const float CSM_PCF_RADIUS           = 1.5;

// Debug cascade colors (up to 4 cascades)
const vec3 CASCADE_DEBUG_COLORS[4] = vec3[](vec3(0.2, 0.8, 0.2), // near
                                            vec3(0.8, 0.8, 0.2),
                                            vec3(0.8, 0.4, 0.2),
                                            vec3(0.8, 0.2, 0.2) // far
);
// Debug mode: 0=off,1=cascade,2=blend,3=depth precision,4=bias
#ifndef CSM_DEBUG_MODE
#define CSM_DEBUG_MODE 0
#endif

// Poisson disk (9 samples, static)
const vec2 POISSON_DISK[9] = vec2[](vec2(0.0, 0.0),
                                    vec2(-0.94201624, -0.39906216),
                                    vec2(0.94558609, -0.76890725),
                                    vec2(-0.09418410, -0.92938870),
                                    vec2(0.34495938, 0.29387760),
                                    vec2(-0.91588581, 0.45771432),
                                    vec2(-0.81544232, -0.87912464),
                                    vec2(0.97484398, 0.75648379),
                                    vec2(0.44323325, -0.97511554));

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// Convert world position to positive view-space depth
float getViewDepth(vec3 worldPos)
{
  return (ubo.view * vec4(worldPos, 1.0)).z;
}

// Cascade boundary helpers
float getCascadeSplitFar(int cascade)
{
  return ubo.directionalCascadeSplits[cascade];
}
float getCascadeSplitNear(int cascade)
{
  return (cascade <= 0) ? 0.0 : getCascadeSplitFar(cascade - 1);
}

// Sub-linear cascade scale
float getCascadeScale(int cascade)
{
  return 1.0 + sqrt(float(cascade));
}

// ============================================================================
// CASCADE SELECTION & BLENDING
// ============================================================================

// Generalized cascade selection
int selectCascade(float viewDepth)
{
  for (int i = 0; i < ubo.directionalCascadeCount; i++)
  {
    if (viewDepth <= ubo.directionalCascadeSplits[i]) return i;
  }
  return ubo.directionalCascadeCount - 1;
}

// Cascade blend factor [0,1], output next cascade index
float calculateCascadeBlendFactor(float viewDepth, int primaryCascade, out int blendCascade)
{
  blendCascade     = -1;
  float blendWidth = ubo.cascadeBlendWidth;
  if (blendWidth < 0.001 || primaryCascade >= ubo.directionalCascadeCount - 1) return 0.0;

  float near       = getCascadeSplitNear(primaryCascade);
  float far        = getCascadeSplitFar(primaryCascade);
  float extent     = far - near;
  float blendStart = far - extent * blendWidth;

  if (viewDepth > blendStart)
  {
    blendCascade = primaryCascade + 1;
    return smoothstep(blendStart, far, viewDepth);
  }
  return 0.0;
}

// ============================================================================
// SHADOW MAP SAMPLING (Directional / PCF / Bias)
// ============================================================================

float sampleShadowMapWithBias(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex, int cascadeIndex, float grazingFade)
{
  if (lightIndex >= ubo.shadowLightCount) return 1.0;

  vec3  N     = normalize(normal);
  vec3  L     = normalize(lightDir);
  float NdotL = max(dot(N, L), 0.0);

  // Normal offset
  float cascadeScale = getCascadeScale(cascadeIndex);
  float sinAngle     = sqrt(1.0 - NdotL * NdotL);
  float normalOffset = CSM_NORMAL_OFFSET_BASE * (1.0 + CSM_NORMAL_OFFSET_GROWTH * (cascadeScale - 1.0)) * sinAngle;
  vec3  offsetPos    = worldPos + N * normalOffset;

  // Light space transform
  vec4 lsPos      = ubo.lightSpaceMatrices[lightIndex] * vec4(offsetPos, 1.0);
  vec3 projCoords = lsPos.xyz / lsPos.w;
  projCoords.xy   = projCoords.xy * 0.5 + 0.5;

  if (projCoords.z < 0.0 || projCoords.z > 1.0 || any(lessThan(projCoords.xy, vec2(0.0))) || any(greaterThan(projCoords.xy, vec2(1.0)))) return 1.0;

  // Bias
  float biasScale = 1.0 + CSM_DEPTH_BIAS_GROWTH * (cascadeScale - 1.0);
  float constBias = CSM_DEPTH_BIAS_BASE * biasScale;
  float slopeBias = CSM_DEPTH_BIAS_SLOPE * biasScale / max(NdotL, CSM_GRAZING_THRESHOLD);
  float totalBias = max(CSM_DEPTH_BIAS_MIN, constBias + slopeBias);

  // PCF sampling
  vec2  texelSize = 1.0 / vec2(textureSize(shadowMaps[lightIndex], 0));
  float shadow    = 0.0;
  for (int i = 0; i < CSM_MAX_POISSON_SAMPLES; i++)
  {
    vec2 uv = projCoords.xy + POISSON_DISK[i] * texelSize * CSM_PCF_RADIUS;
    shadow += texture(shadowMaps[lightIndex], vec3(uv, projCoords.z - totalBias));
  }
  shadow /= float(CSM_MAX_POISSON_SAMPLES);

  return mix(1.0, shadow, grazingFade);
}

// ============================================================================
// DIRECTIONAL CSM SHADOW FUNCTION
// ============================================================================

float calculateDirectionalCSMShadow(vec3 worldPos, vec3 normal, vec3 lightDir)
{
  if (ubo.directionalCascadeCount <= 0) return 1.0;

  vec3  N     = normalize(normal);
  vec3  L     = normalize(lightDir);
  float NdotL = max(dot(N, L), 0.0);

  float grazingFade = smoothstep(CSM_GRAZING_FADE_START, CSM_GRAZING_FADE_END, NdotL);
  if (grazingFade < 0.01) return 1.0;

  float viewDepth         = getViewDepth(worldPos);
  int   primaryCascade    = selectCascade(viewDepth);
  int   primaryLightIndex = ubo.directionalCascadeBaseIndex + primaryCascade;

  float primaryShadow = sampleShadowMapWithBias(worldPos, normal, lightDir, primaryLightIndex, primaryCascade, grazingFade);

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

int getCSMCascadeIndex(vec3 worldPos)
{
  return (ubo.directionalCascadeCount <= 0) ? -1 : selectCascade(getViewDepth(worldPos));
}
float getCSMBlendZoneDebug(vec3 worldPos)
{
  int blend;
  return calculateCascadeBlendFactor(getViewDepth(worldPos), selectCascade(getViewDepth(worldPos)), blend);
}
vec3 getCSMCascadeDebugColor(vec3 worldPos)
{
  int c = getCSMCascadeIndex(worldPos);
  return (c < 0) ? vec3(1.0) : CASCADE_DEBUG_COLORS[clamp(c, 0, 7)];
}
vec3 getCSMDepthPrecisionDebug(vec3 worldPos)
{
  int   cascade = selectCascade(getViewDepth(worldPos));
  float near    = getCascadeSplitNear(cascade);
  float far     = getCascadeSplitFar(cascade);
  float t       = (getViewDepth(worldPos) - near) / max(far - near, 0.001);
  return mix(vec3(0.2, 0.8, 0.2), vec3(0.8, 0.2, 0.2), t);
}
vec3 applyCSMDebugOverlay(vec3 worldPos, float shadow, int debugMode)
{
  if (debugMode == 1)
    return getCSMCascadeDebugColor(worldPos) * shadow;
  else if (debugMode == 2)
  {
    float blend = getCSMBlendZoneDebug(worldPos);
    return mix(getCSMCascadeDebugColor(worldPos), vec3(1.0), blend * 0.5) * shadow;
  }
  else if (debugMode == 3)
    return getCSMDepthPrecisionDebug(worldPos) * shadow;
  return vec3(shadow);
}

// ============================================================================
// SPOT LIGHT SHADOWS
// ============================================================================

float sampleShadowMap(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex)
{
  return sampleShadowMapWithBias(worldPos, normal, lightDir, lightIndex, 0, 1.0);
}
float calculateShadow(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex)
{
  return sampleShadowMap(worldPos, normal, lightDir, lightIndex);
}
float calculateShadow(vec3 worldPos, int lightIndex)
{
  if (lightIndex >= ubo.shadowLightCount) return 1.0;
  vec4 lsPos = ubo.lightSpaceMatrices[lightIndex] * vec4(worldPos, 1.0);
  vec3 proj  = lsPos.xyz / lsPos.w;
  proj.xy    = proj.xy * 0.5 + 0.5;
  if (proj.z < 0.0 || proj.z > 1.0 || any(lessThan(proj.xy, vec2(0.0))) || any(greaterThan(proj.xy, vec2(1.0)))) return 1.0;

  vec2  texelSize = 1.0 / vec2(textureSize(shadowMaps[lightIndex], 0));
  float shadow    = 0.0;
  for (int x = -1; x <= 1; x++)
    for (int y = -1; y <= 1; y++)
    {
      shadow += texture(shadowMaps[lightIndex], vec3(proj.xy + vec2(x, y) * texelSize, proj.z - 0.001));
    }
  return shadow / 9.0;
}

// ============================================================================
// POINT LIGHT SHADOWS
// ============================================================================

float calculatePointLightShadow(vec3 worldPos, int lightIndex)
{
  if (lightIndex >= ubo.cubeShadowLightCount) return 1.0;
  vec3  lightPos = ubo.pointLightShadowData[lightIndex].xyz;
  float farPlane = ubo.pointLightShadowData[lightIndex].w;

  vec3  fragToLight = worldPos - lightPos;
  float depth       = length(fragToLight);
  if (depth > farPlane) return 1.0;

  vec3  dir          = vec3(fragToLight.x, -fragToLight.y, fragToLight.z); // Vulkan cube map convention
  float closestDepth = texture(cubeShadowMaps[lightIndex], dir).r;
  float bias         = 0.02;

  return (depth / farPlane > closestDepth + bias) ? 0.0 : 1.0;
}

#endif // SHADOWS_GLSL