#ifndef SHADOWS_GLSL
#define SHADOWS_GLSL

// Shadow helpers.
// Note: Expects the including shader to define `ubo`, `shadowMaps`, and `cubeShadowMaps`.

// Calculate shadow factor using PCF (Percentage Closer Filtering)
float calculateShadow(vec3 worldPos, int lightIndex) {
    if (lightIndex >= ubo.shadowLightCount)
    return 1.0;

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
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMaps[lightIndex], 0);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            // sampler2DShadow returns 0 or 1 based on depth comparison
            shadow += texture(shadowMaps[lightIndex], vec3(projCoords.xy + offset, projCoords.z));
        }
    }
    shadow /= 9.0;

    return shadow;
}

// Calculate shadow factor for point light using cube shadow map
float calculatePointLightShadow(vec3 worldPos, int lightIndex) {
    if (lightIndex >= ubo.cubeShadowLightCount)
    return 1.0;

    vec3 lightPos = ubo.pointLightShadowData[lightIndex].xyz;
    float farPlane = ubo.pointLightShadowData[lightIndex].w;

    // Direction from light to fragment
    vec3 lightToFrag = worldPos - lightPos;
    float currentDepth = length(lightToFrag);

    // Check if outside light range
    if (currentDepth > farPlane)
    return 1.0;

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
