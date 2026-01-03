#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable

// Compile-time feature toggles.
// Runtime branches do not remove code; build SPIR-V variants with -D flags.
#ifndef PBR_ENABLE_DEBUG
#define PBR_ENABLE_DEBUG 1
#endif
#ifndef PBR_ENABLE_SPEC_GLOSS
#define PBR_ENABLE_SPEC_GLOSS 1
#endif
#ifndef PBR_ENABLE_IRIDESCENCE
#define PBR_ENABLE_IRIDESCENCE 1
#endif
#ifndef PBR_ENABLE_TRANSMISSION
#define PBR_ENABLE_TRANSMISSION 1
#endif
#ifndef PBR_ENABLE_CLEARCOAT
#define PBR_ENABLE_CLEARCOAT 1
#endif
#ifndef PBR_ENABLE_ANISOTROPY
#define PBR_ENABLE_ANISOTROPY 1
#endif

#include "includes/common.glsl"
#include "includes/brdf.glsl"

const float kMaxReflectionLod = 4.0;
const float kMinDistance2     = 1e-8;
const float kMinSpotEpsilon   = 1e-5;
const float kFeatureEps       = 0.01;
const float kMinLightEnergy   = 1e-6;
const float kMinShadowEnergy  = 2e-4;

const int DEBUG_NONE         = 0;
const int DEBUG_ALBEDO       = 1;
const int DEBUG_NORMAL       = 2;
const int DEBUG_ROUGHNESS    = 3;
const int DEBUG_METALLIC     = 4;
const int DEBUG_LIGHTING     = 5;
const int DEBUG_AO           = 6;
const int DEBUG_MESHLETS     = 7;
const int DEBUG_MESHLETCONES = 8;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragmentWorldPos;
layout(location = 2) in vec3 fragmentNormalWorld;
layout(location = 3) in vec2 fragUV;
layout(location = 4) in flat uint inMeshletId;
layout(location = 5) in flat vec3 inConeAxis;

struct PointLight {
    vec4 position;
    vec4 color;
    float radius2;
    float _pad0;
    float _pad1;
    float _pad2;
};

struct DirectionalLight {
    vec4 direction;
    vec4 color;
};

struct SpotLight {
    vec4  position;
    vec4  direction;   // w component is inner cutoff (cos)
    vec4  color;       // w component is intensity
    float outerCutoff; // Outer cutoff (cos)
    float constantAtten;
    float linearAtten;
    float quadraticAtten;
    float radius2;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(set = 0, binding = 0) uniform UBO {
    mat4             proj;
    mat4             view;
    vec4             ambientLightColor;
    vec4             cameraPosition;
    PointLight       pointLights[16];
    DirectionalLight directionalLights[16];
    SpotLight        spotLights[16];
    mat4             lightSpaceMatrices[16];
    vec4             pointLightShadowData[4]; // xyz = position, w = far plane
    int              pointLightCount;
    int              directionalLightCount;
    int              spotLightCount;
    int              shadowLightCount;     // 2D shadow maps (directional + spot)
    int              cubeShadowLightCount; // Cube shadow maps (point lights)
    int              debugMode;            // 0: None, 1: Albedo, 2: Normal, 3: Roughness, 4: Metallic, 5: Lighting
    int              _pad2;
    int              _pad3;
    vec4             frustumPlanes[6];
    vec4             fogColor;       // xyz = Horizon Color, w = density
    vec4             fogZenithColor; // xyz = Zenith Color, w = unused
    float            fogHeight;
    float            fogHeightDensity;
    float            _pad4;
    float            _pad5;
}
ubo;

// Global textures (set 1)
layout(set = 1, binding = 0) uniform sampler2D globalTextures[];

// Shadow maps (set 2) - array of shadow maps for multiple lights
layout(set = 2, binding = 0) uniform sampler2DShadow shadowMaps[4];

// Cube shadow maps for point lights (set 2, binding 1)
layout(set = 2, binding = 1) uniform samplerCube cubeShadowMaps[4];

#include "includes/shadows.glsl"

// IBL textures (set 3)
layout(set = 3, binding = 0) uniform samplerCube irradianceMap;
layout(set = 3, binding = 1) uniform samplerCube prefilterMap;
layout(set = 3, binding = 2) uniform sampler2D brdfLUT;

// Push constants: Only material portion visible in fragment shader (offset 128)
layout(set = 4, binding = 0) uniform MaterialData {
    vec4  albedo;
    vec4  emissiveInfo;             // rgb: color, a: strength
    vec4  specularGlossinessFactor; // rgb: specular, a: glossiness
    vec4  attenuationColorAndDist;  // rgb: color, a: distance
    mat4  params;                   // Packed float parameters
    uvec4 flagsAndIndices0;         // Packed uint parameters
    uvec4 indices1;
    uvec4 indices2;
    uvec4 indices3;
}
material;

vec2 getMaterialUv() {
    return fragUV * material.params[3][1];
}

vec3 evalIridescence(float NdotV, float thickness, float ior) {
    // Simple thin-film interference approximation
    // Phase shift based on path difference
    float cosTheta2  = NdotV * NdotV;
    float sinTheta2  = 1.0 - cosTheta2;
    float sinTheta2t = sinTheta2 / (ior * ior);
    float cosTheta2t = 1.0 - sinTheta2t;
    float cosTheta_t = sqrt(max(0.0, cosTheta2t));

    // Path difference = 2 * n * d * cos(theta_t)
    // We map this to a color cycle
    float pathDiff = 2.0 * ior * thickness * cosTheta_t;

    // Map path difference to RGB phase
    // 400-700nm range roughly
    vec3 phase = vec3(pathDiff) * vec3(1.0 / 450.0, 1.0 / 550.0, 1.0 / 650.0) * 2.0 * PI;

    return vec3(cos(phase.r), cos(phase.g), cos(phase.b)) * 0.5 + 0.5;
}

struct Surface {
    vec3  albedo;
    float alpha;
    float metallic;
    float roughness;
    float ao;
    vec3  N;
    vec3  V;
    vec3  F0;
    vec3  T;
    vec3  B;
    float clearcoatStrength;
    float clearcoatRoughness;
    float anisotropy;
    float NdotV;
    vec3  R;
    float transmission;
};

struct AlphaOnly {
    vec3  albedo;
    float alpha;
};

AlphaOnly computeAlphaOnly() {
    vec2 uv = getMaterialUv();

    vec4  baseColor = material.albedo;
    vec3  albedo    = baseColor.rgb;
    float alpha     = baseColor.a;

    uint materialFlags = material.flagsAndIndices0.x;

    bool hasBaseColorTexture = (materialFlags & (1u << 0)) != 0u;
    bool isAlphaMasked       = (material.flagsAndIndices0.y == 1u);
    bool isOpaque            = (material.flagsAndIndices0.y == 0u);

    if (hasBaseColorTexture) {
        vec4 texColor = texture(globalTextures[nonuniformEXT(material.flagsAndIndices0.z)], uv);
        albedo *= texColor.rgb;
        alpha *= texColor.a;
    }

    // Alpha Masking / Opaque normalization
    if (isAlphaMasked) {
        if (alpha < material.params[3][2]) {
            discard;
        }
        alpha = 1.0;
    }
    else if (isOpaque) {
        alpha = 1.0;
    }

    AlphaOnly result;
    result.albedo = albedo;
    result.alpha  = alpha;
    return result;
}

void calculateDirectLight(Surface surf, vec3 L, vec3 radiance, out vec3 diffuse, out vec3 specular) {
    float NdotL = max(dot(surf.N, L), 0.0);

    if (NdotL <= 0.0) {
        diffuse  = vec3(0.0);
        specular = vec3(0.0);
        return;
    }

    vec3 H = normalize(surf.V + L);

#if PBR_ENABLE_ANISOTROPY
    // Fast-path: use isotropic GGX unless anisotropy is significant.
    float NDF = (abs(surf.anisotropy) > kFeatureEps)
    ? DistributionGGXAnisotropic(surf.N, H, surf.T, surf.B, surf.roughness, surf.anisotropy)
    : DistributionGGX(surf.N, H, surf.roughness);
#else
    float NDF = DistributionGGX(surf.N, H, surf.roughness);
#endif
    float G     = GeometrySmith(surf.NdotV, NdotL, surf.roughness);
    vec3  F     = fresnelSchlick(max(dot(H, surf.V), 0.0), surf.F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - surf.metallic;

    vec3  numerator    = NDF * G * F;
    float denominator  = 4.0 * surf.NdotV * NdotL + 0.0001;
    vec3  specularTerm = numerator / denominator;

    diffuse  = (kD * surf.albedo / PI) * radiance * NdotL;
    specular = specularTerm * radiance * NdotL;
}

vec3 calculateClearcoat(Surface surf, vec3 L, vec3 radiance) {
    vec3  H   = normalize(surf.V + L);
    float NDF = DistributionGGX(surf.N, H, surf.clearcoatRoughness);

    float NdotL = max(dot(surf.N, L), 0.0);
    float G     = GeometrySmith(surf.NdotV, NdotL, surf.clearcoatRoughness);
    vec3  F     = fresnelSchlick(max(dot(H, surf.V), 0.0), vec3(0.04));

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * surf.NdotV * NdotL + 0.0001;
    vec3  specular    = numerator / denominator;

    return specular * radiance * NdotL;
}

void accumulateLight(Surface surf, vec3 L, vec3 radiance, float shadow, bool enableClearcoat, inout vec3 diffuseLo, inout vec3 specularLo, inout vec3 clearcoatLo) {
    vec3 diffuse  = vec3(0.0);
    vec3 specular = vec3(0.0);
    calculateDirectLight(surf, L, radiance, diffuse, specular);

    diffuseLo += diffuse * shadow;
    specularLo += specular * shadow;

    if (enableClearcoat) {
        clearcoatLo += calculateClearcoat(surf, L, radiance) * shadow;
    }
}

Surface getSurfacePropertiesFull(vec3 baseAlbedo, float baseAlpha) {
    // Apply UV tiling scale
    vec2 uv = getMaterialUv();

    // Material properties from textures or push constants
    vec3  albedo             = baseAlbedo;
    float alpha              = baseAlpha;
    float metallic           = material.params[0][0];
    float roughness          = material.params[0][1];
    float ao                 = material.params[0][2];
    float clearcoatStrength  = material.params[1][0];
    float clearcoatRoughness = material.params[1][1];
    float anisotropy         = material.params[1][2];

    uint materialFlags = material.flagsAndIndices0.x;

    bool isAlphaMasked                = (material.flagsAndIndices0.y == 1u);
    bool isOpaque                     = (material.flagsAndIndices0.y == 0u);
#if PBR_ENABLE_SPEC_GLOSS
    bool isSpecularGlossinessWorkflow = (material.indices2.y == 1u);
#else
    bool isSpecularGlossinessWorkflow = false;
#endif

    bool hasNormalMap                     = (materialFlags & (1u << 1)) != 0u;
    bool hasMetallicTexture               = (materialFlags & (1u << 2)) != 0u;
    bool hasRoughnessTexture              = (materialFlags & (1u << 3)) != 0u;
    bool hasAoTexture                     = (materialFlags & (1u << 4)) != 0u;
    bool hasEmissiveTexture               = (materialFlags & (1u << 5)) != 0u;
    bool metallicRoughnessPacked          = (materialFlags & (1u << 6)) != 0u;
    bool occlusionRoughnessMetallicPacked = (materialFlags & (1u << 7)) != 0u;
    bool hasSpecGlossTexture              = (materialFlags & (1u << 8)) != 0u;
    bool hasTransmissionTexture           = (materialFlags & (1u << 9)) != 0u;
    bool hasClearcoatTexture              = (materialFlags & (1u << 10)) != 0u;
    bool hasClearcoatRoughnessTexture     = (materialFlags & (1u << 11)) != 0u;

    bool aoHandled = false;

    if (occlusionRoughnessMetallicPacked) {
        vec4 ormSample = texture(globalTextures[nonuniformEXT(material.indices1.y)], uv);
        ao             = ormSample.r;
        roughness      = ormSample.g;
        metallic       = ormSample.b;
        aoHandled      = true;
    }

    else if (metallicRoughnessPacked) {
        // MetallicRoughness Packed (glTF)
        vec4 mrSample = texture(globalTextures[nonuniformEXT(material.indices1.y)], uv);
        metallic *= mrSample.b;
        roughness *= mrSample.g;
    }

    else {
        float metallicMask  = hasMetallicTexture ? 1.0 : 0.0;
        float roughnessMask = hasRoughnessTexture ? 1.0 : 0.0;

        uint metallicIndex  = hasMetallicTexture ? material.indices1.x : 0u;
        uint roughnessIndex = hasRoughnessTexture ? material.indices1.y : 0u;

        float metallicTex  = texture(globalTextures[nonuniformEXT(metallicIndex)], uv).r;
        float roughnessTex = texture(globalTextures[nonuniformEXT(roughnessIndex)], uv).r;

        // When mask == 0, multiply by 1.0 (no-op)
        metallic *= mix(1.0, metallicTex, metallicMask);
        roughness *= mix(1.0, roughnessTex, roughnessMask);
    }

    if (!aoHandled) {
        float aoMask = hasAoTexture ? 1.0 : 0.0;
        uint  aoIndex = hasAoTexture ? material.indices1.z : 0u;
        float aoTex = texture(globalTextures[nonuniformEXT(aoIndex)], uv).r;
        ao *= mix(1.0, aoTex, aoMask);
    }

    vec3 N = normalize(fragmentNormalWorld);

    // Normal mapping
    if (hasNormalMap) {
        // Normal map
        vec3 tangentNormal = texture(globalTextures[nonuniformEXT(material.flagsAndIndices0.w)], uv).xyz * 2.0 - 1.0;
        tangentNormal.y    = -tangentNormal.y;

        vec3 T, B;
        buildOrthonormalBasis(N, T, B);
        mat3 TBN = mat3(T, B, N);

        N = normalize(TBN * tangentNormal);
    }

    vec3 V = normalize(ubo.cameraPosition.xyz - fragmentWorldPos);
    float NdotV = max(dot(N, V), 0.0);

    // Tangent space for anisotropy (only needed when anisotropy is significant)
    vec3 T = vec3(0.0);
    vec3 B = vec3(0.0);
#if PBR_ENABLE_ANISOTROPY
    if (abs(anisotropy) > kFeatureEps) {
        buildOrthonormalBasis(N, T, B);

        float angle = material.params[1][3] * 2.0 * PI;
        float cosA  = cos(angle);
        float sinA  = sin(angle);
        vec3  Trot  = cosA * T + sinA * B;
        vec3  Brot  = -sinA * T + cosA * B;
        T           = Trot;
        B           = Brot;
    }
#endif

    vec3 F0 = vec3(0.04);

#if PBR_ENABLE_SPEC_GLOSS
    if (isSpecularGlossinessWorkflow) {
        vec3  specularColor = material.specularGlossinessFactor.rgb;
        float glossiness    = material.specularGlossinessFactor.a;

        float specGlossMask = hasSpecGlossTexture ? 1.0 : 0.0;
        uint  specGlossIndex = hasSpecGlossTexture ? material.indices2.x : 0u;
        vec4  sgSample = texture(globalTextures[nonuniformEXT(specGlossIndex)], uv);
        specularColor *= mix(vec3(1.0), sgSample.rgb, specGlossMask);
        glossiness *= mix(1.0, sgSample.a, specGlossMask);

        roughness = 1.0 - glossiness;
        F0        = specularColor;
        metallic  = 0.0;
    }
    else {
#endif
        if (material.params[2][1] != 1.5) {
            float ior = material.params[2][1];
            float f   = (ior - 1.0) / (ior + 1.0);
            F0        = vec3(f * f);
        }
        F0 = mix(F0, albedo, metallic);
#if PBR_ENABLE_SPEC_GLOSS
    }
#endif

#if PBR_ENABLE_IRIDESCENCE
    if (material.params[2][2] > 0.0) {
        vec3 iridescenceColor = evalIridescence(max(NdotV, 0.1), material.params[3][0], material.params[2][3]);
        F0                    = mix(F0, iridescenceColor, material.params[2][2]);
    }
#endif

    // Transmission
#if PBR_ENABLE_TRANSMISSION
    float transmission = material.params[2][0];
    if (hasTransmissionTexture) {
        transmission *= texture(globalTextures[nonuniformEXT(material.indices2.z)], uv).r;
    }
#else
    float transmission = 0.0;
#endif
    // Transmission is disabled for metallic materials
    // transmission *= (1.0 - metallic);

    // Clearcoat
#if PBR_ENABLE_CLEARCOAT
    if (hasClearcoatTexture) {
        clearcoatStrength *= texture(globalTextures[nonuniformEXT(material.indices2.w)], uv).r;
    }
    if (hasClearcoatRoughnessTexture) {
        clearcoatRoughness *= texture(globalTextures[nonuniformEXT(material.indices3.x)], uv).g;
    }
#else
    clearcoatStrength = 0.0;
#endif

    Surface surf;
    surf.albedo             = albedo;
    surf.alpha              = alpha;
    surf.metallic           = metallic;
    surf.roughness          = roughness;
    surf.ao                 = ao;
    surf.N                  = N;
    surf.V                  = V;
    surf.F0                 = F0;
    surf.T                  = T;
    surf.B                  = B;
    surf.clearcoatStrength  = clearcoatStrength;
    surf.clearcoatRoughness = clearcoatRoughness;
    surf.anisotropy         = anisotropy;
    surf.NdotV              = NdotV;
    surf.R                  = reflect(-V, N);
    surf.transmission       = transmission;

    return surf;
}

void calculateIBL(Surface surf, out vec3 outDiffuse, out vec3 outSpecular) {
    vec3 F_IBL       = surf.F0;
    vec3 F_roughness = fresnelSchlickRoughness(surf.NdotV, F_IBL, surf.roughness);

    vec3 kS = F_roughness;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - surf.metallic;

    vec3 irradiance = texture(irradianceMap, surf.N).rgb;
    vec3 diffuse    = irradiance * surf.albedo;

    vec3 prefilteredColor = textureLod(prefilterMap, surf.R, surf.roughness * kMaxReflectionLod).rgb;

    vec2 brdf     = texture(brdfLUT, vec2(surf.NdotV, surf.roughness)).rg;
    vec3 specular = prefilteredColor * (surf.F0 * brdf.x + brdf.y);

    // Horizon occlusion: dampen specular reflection for vectors pointing below the horizon
    float horizon = min(1.0 + dot(surf.R, surf.N), 1.0);
    specular *= horizon * horizon;

    // Specular Occlusion: dampen specular reflection based on AO
    float specularAO = computeSpecularAO(surf.NdotV, surf.ao, surf.roughness);
    specular *= specularAO;

    outDiffuse  = kD * diffuse * surf.ao;
    outSpecular = specular;

    // Add simple ambient as fallback/boost to diffuse
    outDiffuse += ubo.ambientLightColor.xyz * ubo.ambientLightColor.w * surf.albedo * surf.ao * 0.05;
}

vec3 calculateEmissive() {
    vec2 uv       = getMaterialUv();
    vec3 emissive = material.emissiveInfo.rgb * material.emissiveInfo.a;

    uint materialFlags      = material.flagsAndIndices0.x;
    bool hasEmissiveTexture = (materialFlags & (1u << 5)) != 0u;

    float emissiveMask  = hasEmissiveTexture ? 1.0 : 0.0;
    uint  emissiveIndex = hasEmissiveTexture ? material.indices1.w : 0u;
    vec3  emissiveTex   = texture(globalTextures[nonuniformEXT(emissiveIndex)], uv).rgb;
    emissive *= mix(vec3(1.0), emissiveTex, emissiveMask);
    return emissive;
}

void main() {
    AlphaOnly alphaOnly = computeAlphaOnly();
    Surface   surf      = getSurfacePropertiesFull(alphaOnly.albedo, alphaOnly.alpha);

#if PBR_ENABLE_CLEARCOAT
    bool hasClearcoat = (surf.clearcoatStrength > kFeatureEps);
#else
    bool hasClearcoat = false;
#endif

    // Reflectance equation - Base layer
    vec3 diffuseLo   = vec3(0.0);
    vec3 specularLo  = vec3(0.0);
    vec3 clearcoatLo = vec3(0.0);

    // Point lights
    for (int i = 0; i < ubo.pointLightCount; i++) {
        vec3  lightDir  = ubo.pointLights[i].position.xyz - fragmentWorldPos;
        float distance2 = dot(lightDir, lightDir);
        float intensity = ubo.pointLights[i].color.w;

        if (distance2 > ubo.pointLights[i].radius2) continue;

        float invDistance = inversesqrt(max(distance2, kMinDistance2));
        vec3  L           = lightDir * invDistance;
        float attenuation = invDistance * invDistance;
        vec3  radiance    = ubo.pointLights[i].color.xyz * intensity * attenuation;

        // If the light is behind the surface, it cannot contribute; skip shadow sampling too.
        if (dot(surf.N, L) <= 0.0) continue;

        // Extremely small contributions can be skipped entirely.
        float lightEnergy = intensity * attenuation;
        if (lightEnergy <= kMinLightEnergy) continue;

        float shadow = 1.0;
        if (i < ubo.cubeShadowLightCount && lightEnergy > kMinShadowEnergy) {
            shadow = calculatePointLightShadow(fragmentWorldPos, i);
        }

        accumulateLight(surf, L, radiance, shadow, hasClearcoat, diffuseLo, specularLo, clearcoatLo);
    }

    // Directional lights
    for (int i = 0; i < ubo.directionalLightCount; i++) {
        vec3 L        = normalize(-ubo.directionalLights[i].direction.xyz);
        vec3 radiance = ubo.directionalLights[i].color.xyz * ubo.directionalLights[i].color.w;

        if (dot(surf.N, L) <= 0.0) continue;
        if (ubo.directionalLights[i].color.w <= kMinLightEnergy) continue;

        float shadow = 1.0;
        if (i == 0 && ubo.shadowLightCount > 0) {
            shadow = calculateShadow(fragmentWorldPos, 0);
        }

        accumulateLight(surf, L, radiance, shadow, hasClearcoat, diffuseLo, specularLo, clearcoatLo);
    }

    // Spot lights
    for (int i = 0; i < ubo.spotLightCount; i++) {
        vec3  lightDir  = ubo.spotLights[i].position.xyz - fragmentWorldPos;
        float distance2 = dot(lightDir, lightDir);

        if (distance2 > ubo.spotLights[i].radius2) continue;

        float invDist   = inversesqrt(max(distance2, kMinDistance2));
        float distance  = distance2 * invDist;
        vec3  L         = lightDir * invDist;

        if (dot(surf.N, L) <= 0.0) continue;

        vec3  spotDir   = normalize(-ubo.spotLights[i].direction.xyz);
        float theta     = dot(L, spotDir);
        float epsilon   = ubo.spotLights[i].direction.w - ubo.spotLights[i].outerCutoff;
        float intensity = clamp((theta - ubo.spotLights[i].outerCutoff) / max(epsilon, kMinSpotEpsilon), 0.0, 1.0);

        // Outside the cone: skip before attenuation/shadows.
        if (intensity <= 0.0) continue;

        float attenuation = 1.0
        / (ubo.spotLights[i].constantAtten + ubo.spotLights[i].linearAtten * distance
            + ubo.spotLights[i].quadraticAtten * distance2);

        vec3 radiance = ubo.spotLights[i].color.xyz * ubo.spotLights[i].color.w * attenuation * intensity;

        float lightEnergy = ubo.spotLights[i].color.w * attenuation * intensity;
        if (lightEnergy <= kMinLightEnergy) continue;

        int   shadowIndex = 1 + i;
        float shadow      = 1.0;
        if (shadowIndex < ubo.shadowLightCount && lightEnergy > kMinShadowEnergy) {
            shadow = calculateShadow(fragmentWorldPos, shadowIndex);
        }

        accumulateLight(surf, L, radiance, shadow, hasClearcoat, diffuseLo, specularLo, clearcoatLo);
    }

    if (hasClearcoat) {
        specularLo = mix(specularLo, specularLo + clearcoatLo * surf.clearcoatStrength, surf.clearcoatStrength);
    }

    vec3 diffuseIBL, specularIBL;
    calculateIBL(surf, diffuseIBL, specularIBL);

    vec3 emissive = calculateEmissive();

#if PBR_ENABLE_TRANSMISSION
    // --- Advanced Transmission (Volume & Refraction) ---
    vec3 refractedColor = vec3(0.0);
    bool hasTransmission = (surf.transmission > kFeatureEps);
    if (hasTransmission) {
        float thickness           = material.params[3][3];
        vec3  attenuationColor    = material.attenuationColorAndDist.rgb;
        float attenuationDistance = material.attenuationColorAndDist.a;
        float ior                 = material.params[2][1];

        // 1. Volume Attenuation (Beer's Law)
        vec3 volumeTransmission = vec3(1.0);
        if (thickness > 0.0 && attenuationDistance > 0.0) {
            // Avoid log(0) and negative values producing NaNs.
            vec3 safeAttenuationColor = max(attenuationColor, vec3(1e-4));
            vec3 sigma                = -log(safeAttenuationColor) / attenuationDistance;
            volumeTransmission = exp(-sigma * thickness);
        }

        // 2. Refraction (Approximation using IBL)
        // Since we don't have a scene color texture, we use the environment map (prefilterMap)
        // to simulate looking through the object. This gives us "deformation" of the environment.
        vec3 R_refract = refract(-surf.V, surf.N, 1.0 / ior);

        // Check for Total Internal Reflection
        if (length(R_refract) > 0.0) {
            // Sample environment in the refracted direction
            refractedColor = textureLod(prefilterMap, R_refract, surf.roughness * kMaxReflectionLod).rgb;

            // Apply Volume Attenuation
            refractedColor *= volumeTransmission;

            // Tint with Albedo (standard PBR transmission tint)
            refractedColor *= surf.albedo;
        }
    }

#else
    vec3 refractedColor = vec3(0.0);
    bool hasTransmission = false;
#endif

    // Reduce Diffuse contribution based on Transmission
    // (Energy conservation: Light that is transmitted is not reflected as diffuse)
#if PBR_ENABLE_TRANSMISSION
    if (hasTransmission) {
        diffuseLo *= (1.0 - surf.transmission);
        diffuseIBL *= (1.0 - surf.transmission);
    }
#endif

    // Final Composition with Premultiplied Alpha
    // Opacity = alpha * (1 - transmission)
    // Improved Heuristic: Opacity should depend on Albedo Luminance.
    float luminance          = dot(surf.albedo, vec3(0.299, 0.587, 0.114));
    float transmissionFactor = clamp(luminance, 0.3, 0.85);

    float opacity = surf.alpha * (1.0 - surf.transmission * transmissionFactor);

    // Diffuse and Ambient Diffuse are modulated by opacity (background shows through)
    // Specular (Direct + IBL) is additive (sits on top)
    // Emissive is additive
    // Refraction is additive (simulating light coming through)

    vec3 finalColor = (diffuseLo + diffuseIBL) * opacity + (specularLo + specularIBL) + emissive;

    // Add Refraction
    finalColor += refractedColor * surf.transmission;

    if (material.params[0][3] > 0.5) {
        float pulse         = 0.7 + 0.3 * sin(fragmentWorldPos.x + fragmentWorldPos.y + fragmentWorldPos.z);
        float rimIntensity  = 1.0 - abs(dot(surf.N, surf.V));
        rimIntensity        = pow(rimIntensity, 2.0);
        vec3 selectionColor = vec3(1.0, 1.0, 1.0) * pulse * 0.5;
        finalColor += selectionColor * rimIntensity;
    }

#if PBR_ENABLE_DEBUG
    // Debug modes
    switch (ubo.debugMode) {
        case DEBUG_ALBEDO:
        finalColor = surf.albedo;
        opacity    = 1.0;
        break;
        case DEBUG_NORMAL:
        finalColor = surf.N * 0.5 + 0.5;
        opacity    = 1.0;
        break;
        case DEBUG_ROUGHNESS:
        finalColor = vec3(surf.roughness);
        opacity    = 1.0;
        break;
        case DEBUG_METALLIC:
        finalColor = vec3(surf.metallic);
        opacity    = 1.0;
        break;
        case DEBUG_LIGHTING:
        finalColor = (diffuseIBL + specularIBL) + diffuseLo + specularLo;
        opacity    = 1.0;
        break;
        case DEBUG_AO:
        finalColor = vec3(surf.ao);
        opacity    = 1.0;
        break;
        case DEBUG_MESHLETS: {
            uint hash  = inMeshletId;
            hash       = (hash ^ 61) ^ (hash >> 16);
            hash       = hash + (hash << 3);
            hash       = hash ^ (hash >> 4);
            hash       = hash * 0x27d4eb2d;
            hash       = hash ^ (hash >> 15);
            vec3 color = vec3(float(hash & 255), float((hash >> 8) & 255), float((hash >> 16) & 255)) / 255.0;
            finalColor = color;
            opacity    = 1.0;
            break;
        }
        case DEBUG_MESHLETCONES:
        finalColor = inConeAxis * 0.5 + 0.5;
        opacity    = 1.0;
        break;
        case DEBUG_NONE:
        default:
        break;
    }
#endif

    // Apply Fog
    float fogDensity = ubo.fogColor.w;
    if (fogDensity > 0.0) {
        float distance  = length(ubo.cameraPosition.xyz - fragmentWorldPos);
        float fogFactor = 1.0 - exp(-distance * fogDensity);

        // Height Fog
        if (ubo.fogHeightDensity > 0.0) {
            float heightFactor = exp(-(fragmentWorldPos.y - ubo.fogHeight) * ubo.fogHeightDensity);
            fogFactor          = 1.0 - exp(-distance * fogDensity * heightFactor);
        }

        fogFactor = clamp(fogFactor, 0.0, 1.0);

        // Fog Color Mixing (Horizon -> Zenith)
        vec3  rayDir      = normalize(fragmentWorldPos - ubo.cameraPosition.xyz);
        float t           = clamp(rayDir.y, 0.0, 1.0);
        vec3  skyFogColor = mix(ubo.fogColor.rgb, ubo.fogZenithColor.rgb, t);

        finalColor = mix(finalColor, skyFogColor, fogFactor);
    }

    outColor = vec4(finalColor, opacity);
}
