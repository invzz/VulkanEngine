#ifndef SHADOWS_GLSL
#define SHADOWS_GLSL

// ============================================================================
// SHADOW HELPERS
// ============================================================================

const float SHADOW_NORMAL_OFFSET_BASE  = 0.06;
const float SHADOW_DEPTH_BIAS_BASE     = 0.0025;
const float SHADOW_DEPTH_BIAS_SLOPE    = 0.01;
const float SHADOW_DEPTH_BIAS_MIN      = 0.002;
const int   SHADOW_MAX_POISSON_SAMPLES = 9;
const float SHADOW_PCF_RADIUS          = 1.5;

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

const float CASCADE_BLEND_DIST = 0.1;  // 10% blend zone at cascade boundaries

// ============================================================================
// SHADOW MAP SAMPLING (Spot / PCF / Bias)
// ============================================================================

float sampleShadowMapWithBias(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex) {
    if (lightIndex >= ubo.shadowLightCount)
        return 1.0;

    vec3  N     = normalize(normal);
    vec3  L     = normalize(lightDir);
    float NdotL = max(dot(N, L), 0.0);

    // Normal offset
    float sinAngle     = sqrt(1.0 - NdotL * NdotL);
    float normalOffset = SHADOW_NORMAL_OFFSET_BASE * sinAngle;
    vec3  offsetPos    = worldPos + N * normalOffset;

    // Light space transform
    vec4 lsPos      = ubo.lightSpaceMatrices[lightIndex] * vec4(offsetPos, 1.0);
    vec3 projCoords = lsPos.xyz / lsPos.w;
    projCoords.xy   = projCoords.xy * 0.5 + 0.5;

    if (projCoords.z < 0.0 || projCoords.z > 1.0 || any(lessThan(projCoords.xy, vec2(0.0))) || any(greaterThan(projCoords.xy, vec2(1.0))))
        return 1.0;

    // Bias
    float constBias = SHADOW_DEPTH_BIAS_BASE;
    float slopeBias = SHADOW_DEPTH_BIAS_SLOPE / max(NdotL, 0.15);
    float totalBias = max(SHADOW_DEPTH_BIAS_MIN, constBias + slopeBias);

    // PCF sampling
    vec2  texelSize = 1.0 / vec2(textureSize(shadowMaps[lightIndex], 0));
    float shadow    = 0.0;
    for (int i = 0; i < SHADOW_MAX_POISSON_SAMPLES; i++) {
        vec2 uv = projCoords.xy + POISSON_DISK[i] * texelSize * SHADOW_PCF_RADIUS;
        shadow += texture(shadowMaps[lightIndex], vec3(uv, projCoords.z - totalBias));
    }
    shadow /= float(SHADOW_MAX_POISSON_SAMPLES);

    return shadow;
}

// ============================================================================
// SPOT LIGHT SHADOWS
// ============================================================================

float sampleShadowMap(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex) {
    return sampleShadowMapWithBias(worldPos, normal, lightDir, lightIndex);
}
float calculateShadow(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex) {
    return sampleShadowMap(worldPos, normal, lightDir, lightIndex);
}
float calculateShadow(vec3 worldPos, int lightIndex) {
    if (lightIndex >= ubo.shadowLightCount)
        return 1.0;
    vec4 lsPos = ubo.lightSpaceMatrices[lightIndex] * vec4(worldPos, 1.0);
    vec3 proj  = lsPos.xyz / lsPos.w;
    proj.xy    = proj.xy * 0.5 + 0.5;
    if (proj.z < 0.0 || proj.z > 1.0 || any(lessThan(proj.xy, vec2(0.0))) || any(greaterThan(proj.xy, vec2(1.0))))
        return 1.0;

    vec2  texelSize = 1.0 / vec2(textureSize(shadowMaps[lightIndex], 0));
    float shadow    = 0.0;
    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++) {
            shadow += texture(shadowMaps[lightIndex], vec3(proj.xy + vec2(x, y) * texelSize, proj.z - 0.001));
        }
    return shadow / 9.0;
}

// ============================================================================
// POINT LIGHT SHADOWS
// ============================================================================

float calculatePointLightShadow(vec3 worldPos, int lightIndex) {
    if (lightIndex >= ubo.cubeShadowLightCount)
        return 1.0;
    vec3  lightPos = ubo.pointLightShadowData[lightIndex].xyz;
    float farPlane = ubo.pointLightShadowData[lightIndex].w;

    vec3  fragToLight = worldPos - lightPos;
    float depth       = length(fragToLight);
    if (depth > farPlane)
        return 1.0;

    vec3  dir          = vec3(fragToLight.x, -fragToLight.y, fragToLight.z);  // Vulkan cube map convention
    float closestDepth = texture(cubeShadowMaps[lightIndex], dir).r;
    float bias         = 0.02;

    return (depth / farPlane > closestDepth + bias) ? 0.0 : 1.0;
}

// ============================================================================
// CASCADED SHADOW MAPS (Directional Lights)
// ============================================================================

/**
 * @brief Sample a specific cascade shadow map with PCF.
 */
float sampleCascadeShadowMap(vec3 worldPos, vec3 normal, vec3 lightDir, int cascadeIndex) {
    vec3  N     = normalize(normal);
    vec3  L     = normalize(lightDir);
    float NdotL = max(dot(N, L), 0.0);

    // Normal offset
    float sinAngle     = sqrt(1.0 - NdotL * NdotL);
    float normalOffset = SHADOW_NORMAL_OFFSET_BASE * sinAngle;
    vec3  offsetPos    = worldPos + N * normalOffset;

    // Cascade light space
    vec4 lsPos      = ubo.cascadeLightMatrices[cascadeIndex] * vec4(offsetPos, 1.0);
    vec3 projCoords = lsPos.xyz / lsPos.w;
    projCoords.xy   = projCoords.xy * 0.5 + 0.5;

    if (projCoords.z < 0.0 || projCoords.z > 1.0 ||
        any(lessThan(projCoords.xy, vec2(0.0))) || any(greaterThan(projCoords.xy, vec2(1.0))))
        return 1.0;

    // Bias
    float constBias = SHADOW_DEPTH_BIAS_BASE;
    float slopeBias = SHADOW_DEPTH_BIAS_SLOPE / max(NdotL, 0.15);
    float totalBias = max(SHADOW_DEPTH_BIAS_MIN, constBias + slopeBias);

    vec2  texelSize = 1.0 / vec2(textureSize(shadowMaps[cascadeIndex], 0));
    float shadow    = 0.0;
    for (int i = 0; i < SHADOW_MAX_POISSON_SAMPLES; i++) {
        vec2 uv = projCoords.xy + POISSON_DISK[i] * texelSize * SHADOW_PCF_RADIUS;
        shadow += texture(shadowMaps[cascadeIndex], vec3(uv, projCoords.z - totalBias));
    }
    return shadow / float(SHADOW_MAX_POISSON_SAMPLES);
}

/**
 * @brief Compute the directional light shadow factor using CSM.
 * Selects the appropriate cascade based on view-space depth and blends
 * near cascade boundaries to hide seams.
 */
float calculateCascadeShadow(vec3 worldPos, vec3 normal, vec3 lightDir) {
    if (ubo.cascadeCount <= 0)
        return 1.0;

    // View-space depth
    vec4  viewPos   = ubo.view * vec4(worldPos, 1.0);
    float viewDepth = viewPos.z;  // Camera view matrix uses positive-forward convention

    // Select cascade
    int cascadeIndex = 0;
    for (int i = 0; i < ubo.cascadeCount - 1; i++) {
        if (viewDepth > ubo.cascadeSplits[i])
            cascadeIndex = i + 1;
    }

    // Sample the selected cascade
    float shadow = sampleCascadeShadowMap(worldPos, normal, lightDir, cascadeIndex);

    // Blend with next cascade near the split boundary
    if (cascadeIndex < ubo.cascadeCount - 1) {
        float splitDist = ubo.cascadeSplits[cascadeIndex];
        float blendZone = splitDist * CASCADE_BLEND_DIST;
        float t         = (splitDist - viewDepth) / max(blendZone, 1e-6);
        t               = clamp(t, 0.0, 1.0);

        // Only blend if we're near the split boundary
        if (t < 1.0) {
            float nextShadow = sampleCascadeShadowMap(worldPos, normal, lightDir, cascadeIndex + 1);
            shadow = mix(nextShadow, shadow, t);
        }
    }

    return shadow;
}

#endif  // SHADOWS_GLSL