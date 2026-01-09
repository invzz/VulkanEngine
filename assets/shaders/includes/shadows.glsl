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

// Cascaded shadow mapping for the primary directional light.
// Uses ubo.directionalCascadeSplits (view-space Z distances) and the matrices stored
// in ubo.lightSpaceMatrices starting at ubo.directionalCascadeBaseIndex.
float calculateDirectionalCSMShadow(vec3 worldPos, vec3 normal, vec3 lightDir) {
    if (ubo.directionalCascadeCount <= 0) return 1.0;

    // View-space depth in this engine is +Z forward.
    float viewDepth = (ubo.view * vec4(worldPos, 1.0)).z;
    viewDepth       = max(viewDepth, 0.0);

    vec4 splits  = ubo.directionalCascadeSplits;
    int  cascade = 0;

    // NOTE: directionCascadeCount is expected to be <= 4.
    if (ubo.directionalCascadeCount > 1 && viewDepth > splits.x) cascade = 1;
    if (ubo.directionalCascadeCount > 2 && viewDepth > splits.y) cascade = 2;
    if (ubo.directionalCascadeCount > 3 && viewDepth > splits.z) cascade = 3;

    cascade        = clamp(cascade, 0, ubo.directionalCascadeCount - 1);
    int lightIndex = ubo.directionalCascadeBaseIndex + cascade;
    return calculateShadow(worldPos, normal, lightDir, lightIndex);
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
