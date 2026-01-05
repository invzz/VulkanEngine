#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable

// Thin compositor shader:
// - Opaque lighting/reflections are handled in the deferred lighting pass.
// - This shader only composites:
//   - transmission (screen-space refraction vs sceneColor reflection, no fixed-function blending)
//   - alpha blend (premultiplied coverage, fixed-function blending ONE / ONE_MINUS_SRC_ALPHA)

// Compile-time feature toggles used by compile_shaders.ps1 for a reduced "standard" variant.
#ifndef PBR_ENABLE_TRANSMISSION
#define PBR_ENABLE_TRANSMISSION 1
#endif

#include "includes/common.glsl"
#include "includes/brdf.glsl"
#include "includes/scene_ubo.glsl"
#include "includes/material_decode.glsl"

const float kFeatureEps = 0.01;

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragmentWorldPos;
layout(location = 2) in vec3 fragmentNormalWorld;
layout(location = 3) in vec2 fragUV;
layout(location = 4) in flat uint inMeshletId;
layout(location = 5) in flat vec3 inConeAxis;

// Scene color copy (set 5): contains already-lit HDR (incl. skybox) for refraction + reflection.
layout(set = 5, binding = 0) uniform sampler2D sceneColor;

struct SurfaceLite {
    vec3  albedo;
    float alpha;
    float metallic;
    float roughness;
    vec3  N;
    vec3  V;
    vec3  F0;
    float transmission;
};

SurfaceLite getSurfaceCompositor(AlphaOnly alphaOnly) {
    vec2 uv = material_getUv(fragUV);

    float metallic  = material.params[0][0];
    float roughness = material.params[0][1];
    material_decodeMetallicRoughness(uv, metallic, roughness);

    vec3 N = material_decodeNormalWorld(uv, fragmentNormalWorld);

    vec3 V = normalize(ubo.cameraPosition.xyz - fragmentWorldPos);

    vec3 F0 = material_computeF0(alphaOnly.albedo, metallic);
    float transmission = material_decodeTransmission(uv);

    SurfaceLite surf;
    surf.albedo       = alphaOnly.albedo;
    surf.alpha        = alphaOnly.alpha;
    surf.metallic     = metallic;
    surf.roughness    = roughness;
    surf.N            = N;
    surf.V            = V;
    surf.F0           = F0;
    surf.transmission = transmission;
    return surf;
}

vec2 getScreenUv() {
    vec2 size = vec2(textureSize(sceneColor, 0));
    return clamp(gl_FragCoord.xy / max(size, vec2(1.0)), vec2(0.0), vec2(1.0));
}

#if PBR_ENABLE_TRANSMISSION
vec3 sampleRefractedSceneColor(SurfaceLite surf) {
    float thickness           = material.params[3][3];
    vec3  attenuationColor    = material.attenuationColorAndDist.rgb;
    float attenuationDistance = material.attenuationColorAndDist.a;
    float ior                 = material.params[2][1];

    vec3 Nf = (dot(surf.N, surf.V) < 0.0) ? -surf.N : surf.N;

    vec3 volumeTransmission = vec3(1.0);
    if (thickness > 0.0 && attenuationDistance > 0.0) {
        vec3 safeAttenuationColor = max(attenuationColor, vec3(1e-4));
        vec3 sigma = -log(safeAttenuationColor) / attenuationDistance;
        volumeTransmission = exp(-sigma * thickness);
    }

    vec3 refractedDir = refract(-surf.V, Nf, 1.0 / max(ior, 1.0001));
    float hasRefract = step(1e-6, dot(refractedDir, refractedDir));

    vec3 refractPos = fragmentWorldPos + refractedDir * thickness;
    vec4 clip = ubo.proj * ubo.view * vec4(refractPos, 1.0);

    vec2 refractUv = getScreenUv();
    if (clip.w > 1e-6) {
        vec2 ndc = clip.xy / clip.w;
        refractUv = clamp(ndc * 0.5 + 0.5, vec2(0.0), vec2(1.0));
    }

    float sceneMaxLod = max(float(textureQueryLevels(sceneColor)) - 1.0, 0.0);
    float sceneLod    = clamp(surf.roughness * sceneMaxLod, 0.0, sceneMaxLod);
    vec3  sceneSample = textureLod(sceneColor, refractUv, sceneLod).rgb;

    return sceneSample * volumeTransmission * surf.albedo * hasRefract;
}
#endif

void main() {
    vec2 uv = material_getUv(fragUV);
    AlphaOnly alphaOnly = material_computeAlphaOnly(uv);
    SurfaceLite surf = getSurfaceCompositor(alphaOnly);
    vec3 emissive = material_decodeEmissive(uv);

    bool isAlphaBlend = (material.flagsAndIndices0.y == 2u);

    float effectiveTransmission = 0.0;
#if PBR_ENABLE_TRANSMISSION
    bool hasTransmission = (surf.transmission > kFeatureEps);
    effectiveTransmission = hasTransmission ? (surf.transmission * (1.0 - surf.metallic)) : 0.0;

    // Transmission compositor (fixed-function blending is disabled for the transmission pipeline).
    if (hasTransmission) {
        vec2 screenUv  = getScreenUv();
        vec3 reflected = texture(sceneColor, screenUv).rgb;
        vec3 transmitted = sampleRefractedSceneColor(surf);

        vec3 Nf = (dot(surf.N, surf.V) < 0.0) ? -surf.N : surf.N;
        vec3 F = fresnelSchlick(clamp(dot(Nf, surf.V), 0.0, 1.0), surf.F0);

        vec3 composite = mix(transmitted, reflected, F);
        vec3 finalColor = mix(reflected, composite, effectiveTransmission) + emissive;
        outColor = vec4(finalColor, 1.0);
        return;
    }
#endif

    // Alpha-blend compositor: premultiplied coverage (pipeline uses ONE / ONE_MINUS_SRC_ALPHA).
    if (isAlphaBlend) {
        float a = clamp(surf.alpha, 0.0, 1.0);
        vec3 rgb = (surf.albedo * a) + emissive;
        outColor = vec4(rgb, a);
        return;
    }

    // Fallback for non-hybrid forward paths: unlit albedo + emissive.
    outColor = vec4(surf.albedo + emissive, 1.0);
}
