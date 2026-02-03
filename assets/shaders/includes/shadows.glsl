#ifndef SHADOWS_GLSL
#define SHADOWS_GLSL

// Shadow helpers.
// Note: Expects the including shader to define `ubo`, `shadowMaps`, and `cubeShadowMaps`.

// Calculate shadow factor using PCF (Percentage Closer Filtering)
float calculateShadow(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex) {
    if (lightIndex >= ubo.shadowLightCount) return 1.0;

    // Transform world position to light space
    vec4 lightSpacePos = ubo.lightSpaceMatrices[lightIndex] * vec4(worldPos, 1.0);

    // Perspective divide (needed for spotlight perspective projection)
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // Transform from [-1,1] to [0,1] for UV lookup (Vulkan already has Z in [0,1])
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Check if outside shadow map bounds
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0; // No shadow outside light frustum
    }

    // PCF 3x3 sampling for soft shadows
    float shadow    = 0.0;
    vec2  texelSize = 1.0 / textureSize(shadowMaps[lightIndex], 0);

    // Slope-scaled bias to reduce shadow acne at grazing angles.
    // Depth is in [0,1] here; scale bias with texel size so it adapts to resolution.
    float NdotL        = clamp(dot(normalize(normal), normalize(lightDir)), 0.0, 1.0);
    float minBias      = 0.00025;
    float slopeBias    = (1.0 - NdotL) * (2.0 * max(texelSize.x, texelSize.y));
    float bias         = max(minBias, slopeBias);
    float compareDepth = clamp(projCoords.z - bias, 0.0, 1.0);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            // sampler2DShadow returns 0 or 1 based on depth comparison
            shadow += texture(shadowMaps[lightIndex], vec3(projCoords.xy + offset, compareDepth));
        }
    }
    shadow /= 9.0;

    return shadow;
}

// Poisson disk samples for smoother PCF shadows (16 samples)
const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),  vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),   vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),   vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),   vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),  vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),   vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),    vec2(0.14383161, -0.14100790)
);

// CSM shadow with world-space normal offset (cascade-independent)
float calculateCSMShadowInternal(vec3 worldPos, vec3 normal, vec3 lightDir, int lightIndex, float normalOffset) {
    if (lightIndex >= ubo.shadowLightCount) return 1.0;

    // Apply world-space normal offset (same for all cascades = no seams)
    vec3 offsetPos = worldPos + normal * normalOffset;

    vec4 lightSpacePos = ubo.lightSpaceMatrices[lightIndex] * vec4(offsetPos, 1.0);
    vec3 projCoords    = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy      = projCoords.xy * 0.5 + 0.5;

    // Soft boundary fadeout instead of hard cutoff
    float edgeFade = 1.0;
    float margin   = 0.02;
    if (projCoords.x < margin) edgeFade *= smoothstep(0.0, margin, projCoords.x);
    if (projCoords.x > 1.0 - margin) edgeFade *= smoothstep(0.0, margin, 1.0 - projCoords.x);
    if (projCoords.y < margin) edgeFade *= smoothstep(0.0, margin, projCoords.y);
    if (projCoords.y > 1.0 - margin) edgeFade *= smoothstep(0.0, margin, 1.0 - projCoords.y);

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0;
    }

    vec2 texelSize = 1.0 / textureSize(shadowMaps[lightIndex], 0);

    // Fixed depth bias (small, consistent)
    float compareDepth = clamp(projCoords.z - 0.002, 0.0, 1.0);

    // PCF with 16-tap Poisson disk
    float shadow       = 0.0;
    float filterRadius = 2.0;
    for (int i = 0; i < 16; i++) {
        vec2 offset = poissonDisk[i] * texelSize * filterRadius;
        shadow += texture(shadowMaps[lightIndex], vec3(projCoords.xy + offset, compareDepth));
    }
    shadow /= 16.0;

    // Apply edge fade
    return mix(1.0, shadow, edgeFade);
}

// Cascaded shadow mapping for the primary directional light.
// Uses ubo.directionalCascadeSplits (view-space Z distances) and the matrices stored
// in ubo.lightSpaceMatrices starting at ubo.directionalCascadeBaseIndex.

// Returns the cascade index for debugging purposes
int getCSMCascadeIndex(vec3 worldPos) {
    if (ubo.directionalCascadeCount <= 0) return -1;

    // View-space depth: this engine uses +Z forward, so view-space Z is positive
    // for objects in front of the camera.
    float viewDepth = (ubo.view * vec4(worldPos, 1.0)).z;

    vec4 splits  = ubo.directionalCascadeSplits;
    int  cascade = 0;

    // Select cascade based on view-space depth
    if (ubo.directionalCascadeCount > 1 && viewDepth > splits.x) cascade = 1;
    if (ubo.directionalCascadeCount > 2 && viewDepth > splits.y) cascade = 2;
    if (ubo.directionalCascadeCount > 3 && viewDepth > splits.z) cascade = 3;

    return clamp(cascade, 0, ubo.directionalCascadeCount - 1);
}

// Returns view-space depth for debugging
float getCSMViewDepth(vec3 worldPos) {
    return (ubo.view * vec4(worldPos, 1.0)).z;
}

float calculateDirectionalCSMShadow(vec3 worldPos, vec3 normal, vec3 lightDir) {
    if (ubo.directionalCascadeCount <= 0) return 1.0;

    float viewDepth = (ubo.view * vec4(worldPos, 1.0)).z;
    vec4  splits    = ubo.directionalCascadeSplits;

    // Calculate world-space normal offset ONCE (cascade-independent!)
    float NdotL        = clamp(dot(normalize(normal), normalize(lightDir)), 0.0, 1.0);
    float normalOffset = (1.0 - NdotL) * 0.05;  // Fixed world-space offset (5cm max)

    // Find which cascade we're in and calculate blend factor
    // We always sample two adjacent cascades and blend between them
    int   cascade0    = 0;
    int   cascade1    = 0;
    float blendFactor = 0.0;

    // Determine cascades and blend factor based on view depth
    if (ubo.directionalCascadeCount == 1)
    {
        cascade0 = 0;
        cascade1 = 0;
        blendFactor = 0.0;
    }
    else if (viewDepth <= splits.x)
    {
        cascade0 = 0;
        cascade1 = 1;
        // Blend in the last 30% of cascade 0
        float blendStart = splits.x * 0.7;
        blendFactor = (viewDepth > blendStart) ? smoothstep(0.0, 1.0, (viewDepth - blendStart) / (splits.x - blendStart)) : 0.0;
    }
    else if (ubo.directionalCascadeCount > 2 && viewDepth <= splits.y)
    {
        cascade0 = 1;
        cascade1 = 2;
        float range = splits.y - splits.x;
        float blendStart = splits.x + range * 0.7;
        blendFactor = (viewDepth > blendStart) ? smoothstep(0.0, 1.0, (viewDepth - blendStart) / (splits.y - blendStart)) : 0.0;
    }
    else if (ubo.directionalCascadeCount > 3 && viewDepth <= splits.z)
    {
        cascade0 = 2;
        cascade1 = 3;
        float range = splits.z - splits.y;
        float blendStart = splits.y + range * 0.7;
        blendFactor = (viewDepth > blendStart) ? smoothstep(0.0, 1.0, (viewDepth - blendStart) / (splits.z - blendStart)) : 0.0;
    }
    else
    {
        // Beyond all splits or in last cascade
        cascade0 = min(ubo.directionalCascadeCount - 1, 3);
        cascade1 = cascade0;
        blendFactor = 0.0;
    }

    // Clamp cascade indices
    cascade0 = clamp(cascade0, 0, ubo.directionalCascadeCount - 1);
    cascade1 = clamp(cascade1, 0, ubo.directionalCascadeCount - 1);

    // Sample shadow from primary cascade
    int   lightIndex0 = ubo.directionalCascadeBaseIndex + cascade0;
    float shadow0     = calculateCSMShadowInternal(worldPos, normal, lightDir, lightIndex0, normalOffset);

    // If blending, sample from next cascade too
    if (blendFactor > 0.0 && cascade0 != cascade1)
    {
        int   lightIndex1 = ubo.directionalCascadeBaseIndex + cascade1;
        float shadow1     = calculateCSMShadowInternal(worldPos, normal, lightDir, lightIndex1, normalOffset);
        
        // Use max to hide seams - always pick less shadow when cascades disagree
        return mix(shadow0, max(shadow0, shadow1), blendFactor);
    }

    return shadow0;
}

// Backwards-compatible wrapper (no normal/lightDir available). Uses a small constant bias.
float calculateShadow(vec3 worldPos, int lightIndex) {
    if (lightIndex >= ubo.shadowLightCount) return 1.0;

    vec4 lightSpacePos = ubo.lightSpaceMatrices[lightIndex] * vec4(worldPos, 1.0);
    vec3 projCoords    = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy      = projCoords.xy * 0.5 + 0.5;
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z < 0.0 || projCoords.z > 1.0) {
        return 1.0;
    }

    vec2  texelSize    = 1.0 / textureSize(shadowMaps[lightIndex], 0);
    float compareDepth = clamp(projCoords.z - 0.0005, 0.0, 1.0);

    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            shadow += texture(shadowMaps[lightIndex], vec3(projCoords.xy + offset, compareDepth));
        }
    }
    return shadow / 9.0;
}

// Calculate shadow factor for point light using cube shadow map
float calculatePointLightShadow(vec3 worldPos, int lightIndex) {
    if (lightIndex >= ubo.cubeShadowLightCount) return 1.0;

    vec3  lightPos = ubo.pointLightShadowData[lightIndex].xyz;
    float farPlane = ubo.pointLightShadowData[lightIndex].w;

    // Direction from light to fragment
    vec3  lightToFrag  = worldPos - lightPos;
    float currentDepth = length(lightToFrag);

    // Check if outside light range
    if (currentDepth > farPlane) return 1.0;

    // For Vulkan cube maps, flip Y to match the rendering coordinate system
    vec3 sampleDir = vec3(lightToFrag.x, -lightToFrag.y, lightToFrag.z);

    // Sample cube shadow map - stored value is linear depth / farPlane
    float closestDepth = texture(cubeShadowMaps[lightIndex], sampleDir).r;

    // Normalize current depth to [0, 1] range
    float normalizedDepth = currentDepth / farPlane;

    // Bias to prevent shadow acne
    float bias = 0.02;

    // In shadow if current fragment is further than stored depth
    return (normalizedDepth > closestDepth + bias) ? 0.0 : 1.0;
}

#endif // SHADOWS_GLSL
