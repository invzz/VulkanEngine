#ifndef SHADOWS_GLSL
#define SHADOWS_GLSL

// ============================================================================
// SHADOW HELPERS
// ============================================================================

// Bias values assume hardware depth bias is enabled (constant=1.5, slope=2.0)
// and back-face culling is active during the shadow pass. The hardware bias
// handles most of the slope-scale correction, so the shader-side values can
// stay small to minimize Peter Panning.
const float SHADOW_NORMAL_OFFSET_BASE = 0.02;
const float SHADOW_SHADER_BIAS       = 0.0005;

const int   SHADOW_MAX_POISSON_SAMPLES = 9;
const float SHADOW_PCF_RADIUS          = 1.2;

// Poisson disk (9 samples) — distribution covers the disk evenly.
const vec2 POISSON_DISK[9] = vec2[](
    vec2( 0.0000,  0.0000),
    vec2( 0.5134,  0.1581),
    vec2(-0.3913,  0.5486),
    vec2( 0.1877, -0.5932),
    vec2(-0.6520, -0.2705),
    vec2( 0.7147, -0.4065),
    vec2(-0.2166,  0.7812),
    vec2( 0.8933,  0.2321),
    vec2(-0.7521, -0.5148)
);

const float CASCADE_BLEND_DIST = 0.3;  // 30% blend zone at cascade boundaries

// ============================================================================
// FRAGMENT HASH — rotate Poisson samples per pixel to break up patterns
// ============================================================================

vec2 hash2D(vec2 p) {
    p = fract(p * vec2(443.8975, 397.2973));
    p += dot(p, p + 19.19);
    return fract(p * vec2(1.0 / 7.0, 1.0 / 13.0));
}

// ============================================================================
// SHADOW MAP SAMPLING (Spot / PCF / Bias)
// ============================================================================

float sampleShadowMapWithBias(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex) {
    if (lightIndex >= ubo.shadowLightCount)
        return 1.0;

    vec3  N     = normalize(normal);
    vec3  L     = normalize(lightDir);
    float NdotL = max(dot(N, L), 0.0);

    // Normal offset (small — hardware depth bias handles most of the work)
    float sinAngle     = sqrt(1.0 - NdotL * NdotL);
    float normalOffset = SHADOW_NORMAL_OFFSET_BASE * sinAngle;
    vec3  offsetPos    = worldPos + N * normalOffset;

    // Light space transform
    vec4 lsPos      = ubo.lightSpaceMatrices[lightIndex] * vec4(offsetPos, 1.0);
    vec3 projCoords = lsPos.xyz / lsPos.w;
    projCoords.xy   = projCoords.xy * 0.5 + 0.5;

    if (projCoords.z < 0.0 || projCoords.z > 1.0 || any(lessThan(projCoords.xy, vec2(0.0))) || any(greaterThan(projCoords.xy, vec2(1.0))))
        return 1.0;

    // Rotated Poisson PCF
    vec2  rotation = hash2D(gl_FragCoord.xy);
    float cosR     = cos(rotation.x * 6.2832);
    float sinR     = sin(rotation.x * 6.2832);

    vec2  texelSize = 1.0 / vec2(textureSize(shadowMaps[lightIndex], 0));
    float shadow    = 0.0;
    for (int i = 0; i < SHADOW_MAX_POISSON_SAMPLES; i++) {
        vec2 rotated = vec2(
            POISSON_DISK[i].x * cosR - POISSON_DISK[i].y * sinR,
            POISSON_DISK[i].x * sinR + POISSON_DISK[i].y * cosR
        );
        vec2 uv = projCoords.xy + rotated * texelSize * SHADOW_PCF_RADIUS;
        shadow += texture(shadowMaps[lightIndex], vec3(uv, projCoords.z - SHADOW_SHADER_BIAS));
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
    vec2  rotation  = hash2D(gl_FragCoord.xy);
    float cosR      = cos(rotation.x * 6.2832);
    float sinR      = sin(rotation.x * 6.2832);

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_MAX_POISSON_SAMPLES; i++) {
        vec2 rotated = vec2(
            POISSON_DISK[i].x * cosR - POISSON_DISK[i].y * sinR,
            POISSON_DISK[i].x * sinR + POISSON_DISK[i].y * cosR
        );
        vec2 uv = proj.xy + rotated * texelSize * SHADOW_PCF_RADIUS;
        shadow += texture(shadowMaps[lightIndex], vec3(uv, proj.z - SHADOW_SHADER_BIAS));
    }
    return shadow / float(SHADOW_MAX_POISSON_SAMPLES);
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
    float bias         = 0.01;

    return (depth / farPlane > closestDepth + bias) ? 0.0 : 1.0;
}

// ============================================================================
// CASCADED SHADOW MAPS (Directional Lights)
// ============================================================================

/**
 * @brief Sample a specific cascade shadow map with rotated Poisson PCF.
 */
float sampleCascadeShadowMap(vec3 worldPos, vec3 normal, vec3 lightDir, int cascadeIndex) {
    vec3  N     = normalize(normal);
    vec3  L     = normalize(lightDir);
    float NdotL = max(dot(N, L), 0.0);

    // Small normal offset — hardware depth bias + back-face culling handles most
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

    // Rotated Poisson PCF
    vec2  rotation = hash2D(gl_FragCoord.xy);
    float cosR     = cos(rotation.x * 6.2832);
    float sinR     = sin(rotation.x * 6.2832);

    vec2  texelSize = 1.0 / vec2(textureSize(shadowMaps[cascadeIndex], 0));
    float shadow    = 0.0;
    for (int i = 0; i < SHADOW_MAX_POISSON_SAMPLES; i++) {
        vec2 rotated = vec2(
            POISSON_DISK[i].x * cosR - POISSON_DISK[i].y * sinR,
            POISSON_DISK[i].x * sinR + POISSON_DISK[i].y * cosR
        );
        vec2 uv = projCoords.xy + rotated * texelSize * SHADOW_PCF_RADIUS;
        shadow += texture(shadowMaps[cascadeIndex], vec3(uv, projCoords.z - SHADOW_SHADER_BIAS));
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
    float viewDepth = viewPos.z;

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
        // Smooth-step the blend to hide the transition better
        t = t * t * (3.0 - 2.0 * t);

        if (t < 1.0) {
            float nextShadow = sampleCascadeShadowMap(worldPos, normal, lightDir, cascadeIndex + 1);
            shadow = mix(nextShadow, shadow, t);
        }
    }

    return shadow;
}

#endif  // SHADOWS_GLSL