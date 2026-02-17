#version 450
#extension GL_GOOGLE_include_directive : enable

#include "includes/brdf.glsl"
#include "includes/scene_ubo.glsl"

/*==============================================================================
  Constants
==============================================================================*/

#define MAX_REFLECTION_LOD 4.0

/* Debug modes */
const int DEBUG_ALBEDO         = 1;
const int DEBUG_NORMAL         = 2;
const int DEBUG_ROUGHNESS      = 3;
const int DEBUG_METALLIC       = 4;
const int DEBUG_LIGHTING_ONLY  = 5;
const int DEBUG_EMISSIVE_ONLY  = 6;
const int DEBUG_MESHLETS       = 7;
const int DEBUG_MESHLET_CONES  = 8;
const int DEBUG_DEPTH          = 9;
const int DEBUG_AO             = 10;
const int DEBUG_IBL_DIFFUSE    = 12;
const int DEBUG_IBL_SPECULAR   = 13;
const int DEBUG_BRDF_LUT       = 14;
const int DEBUG_CSM_CASCADES   = 15;
const int DEBUG_CSM_VIEW_DEPTH = 16;
const int DEBUG_CSM_SPLITS     = 17;
const int DEBUG_CSM_DEPTH_HUE  = 18;
const int DEBUG_CSM_SAMPLES    = 19;

/*==============================================================================
  I/O
==============================================================================*/

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

/* G-buffer */
layout(set = 1, binding = 0) uniform sampler2D gbufferNormal;
layout(set = 1, binding = 1) uniform sampler2D gbufferAlbedo;
layout(set = 1, binding = 2) uniform sampler2D gbufferMaterial;
layout(set = 1, binding = 3) uniform sampler2D depthMap;

/* IBL */
layout(set = 3, binding = 0) uniform samplerCube irradianceMap;
layout(set = 3, binding = 1) uniform samplerCube prefilterMap;
layout(set = 3, binding = 2) uniform sampler2D brdfLUT;

/* Shadows */
layout(set = 2, binding = 0) uniform sampler2DShadow shadowMaps[8];
layout(set = 2, binding = 1) uniform samplerCube cubeShadowMaps[4];

#include "includes/shadows.glsl"

/*==============================================================================
  Structs
==============================================================================*/

struct Surface {
    vec3 worldPos;
    vec3 N;
    vec3 V;

    vec3  albedo;
    float metallic;
    float roughness;
    float ao;

    vec3  F0;
    float NdotV;
};

struct LightingResult {
    vec3 diffuse;
    vec3 specular;
};

/*==============================================================================
  Utilities
==============================================================================*/

vec3 octDecode(vec2 e) {
    vec3 v = vec3(e, 1.0 - abs(e.x) - abs(e.y));
    if (v.z < 0.0)
        v.xy = (1.0 - abs(v.yx)) * sign(v.xy);
    return normalize(v);
}

float iorToF0(float ior) {
    ior     = max(ior, 1e-3);
    float a = (ior - 1.0) / (ior + 1.0);
    return a * a;
}

vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 view = ubo.invProj * clip;
    view /= max(view.w, 1e-6);
    return (ubo.invView * view).xyz;
}

vec3 applyIridescenceToF0(vec3 F0, float metallic, float NdotV, float iridescenceFactor, float iridescenceIOR, float iridescenceThicknessNm) {
    float strength = clamp(iridescenceFactor, 0.0, 1.0);

    float ior       = max(iridescenceIOR, 1.0);
    float thickness = max(iridescenceThicknessNm, 0.0);
    float cosThetaI = clamp(NdotV, 0.0, 1.0);

    float sinThetaI2 = max(1.0 - cosThetaI * cosThetaI, 0.0);
    float sinThetaT2 = sinThetaI2 / max(ior * ior, 1e-6);
    float cosThetaT  = sqrt(max(1.0 - sinThetaT2, 0.0));

    vec3 lambda = vec3(650.0, 510.0, 475.0);

    vec3 phase = 4.0 * PI * ior * thickness * cosThetaT / lambda;
    phase      = mod(phase, 2.0 * PI);

    vec3 interference = 0.5 + 0.5 * cos(phase);

    float avg    = dot(interference, vec3(1.0 / 3.0));
    vec3  chroma = interference - vec3(avg);

    float f0Film = pow((ior - 1.0) / (ior + 1.0), 2.0);

    float isMetal = metallic > 0.5 ? 1.0 : 0.0;
    vec3  baseF0  = mix(vec3(f0Film), F0, isMetal);

    vec3 targetF0 = clamp(baseF0 + chroma * 0.35, 0.0, 1.0);

    return mix(F0, targetF0, strength);
}

/*==============================================================================
  Surface decoding
==============================================================================*/

Surface loadSurface(vec2 uv, float depth, out bool isDebugPrimitive) {
    Surface s;

    s.worldPos = reconstructWorldPos(uv, depth);
    s.V        = normalize(ubo.cameraPosition.xyz - s.worldPos);

    vec4 nPacked = texture(gbufferNormal, uv);
    s.N          = octDecode(nPacked.rg * 2.0 - 1.0);

    float materialIOR    = nPacked.b;
    float iridescenceIOR = nPacked.a;

    vec4 albedoA = texture(gbufferAlbedo, uv);
    s.albedo     = albedoA.rgb;

    float iridescenceThickness = albedoA.a;

    isDebugPrimitive = (iridescenceThickness < 0.0);

    vec4 mat          = texture(gbufferMaterial, uv);
    s.metallic        = mat.r;
    s.roughness       = mat.g;
    s.ao              = mat.b;
    float iridescence = mat.a;

    s.NdotV = max(dot(s.N, s.V), 0.0);

    float dielectricF0 = iorToF0(materialIOR);
    s.F0               = mix(vec3(dielectricF0), s.albedo, s.metallic);

    s.F0 = applyIridescenceToF0(s.F0, s.metallic, s.NdotV, iridescence, iridescenceIOR, iridescenceThickness);

    return s;
}

/*==============================================================================
  Attenuation (shared by point + spot)
==============================================================================*/

float computeDistanceAttenuation(float dist2, float radius2) {
    if (radius2 > 0.0 && dist2 > radius2)
        return 0.0;

    float fade = (radius2 > 0.0) ? clamp(1.0 - dist2 / radius2, 0.0, 1.0) : 1.0;

    return (fade * fade) / max(dist2, 1e-4);
}

/*==============================================================================
  IBL
==============================================================================*/

LightingResult computeIBL(Surface s) {
    LightingResult r;

    vec3 R = reflect(-s.V, s.N);

    vec3 F  = fresnelSchlickRoughness(s.NdotV, s.F0, s.roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - s.metallic);

    vec3 irradiance = texture(irradianceMap, s.N).rgb;
    r.diffuse       = kD * irradiance * s.albedo * s.ao;

    vec3 prefiltered = textureLod(prefilterMap, R, s.roughness * MAX_REFLECTION_LOD).rgb;

    vec2 brdf  = texture(brdfLUT, vec2(s.NdotV, s.roughness)).rg;
    r.specular = prefiltered * (s.F0 * brdf.x + brdf.y);

    float horizon = min(1.0 + dot(R, s.N), 1.0);
    r.specular *= horizon * horizon;
    r.specular *= computeSpecularAO(s.NdotV, s.ao, s.roughness);

    return r;
}

/*==============================================================================
  Debug handling
==============================================================================*/

bool handleDebug(Surface s, vec3 iblDiffuse, vec3 iblSpecular) {
    switch (ubo.debugMode) {
        case DEBUG_ALBEDO:
            outColor = vec4(s.albedo, 1.0);
            return true;
        case DEBUG_NORMAL:
            outColor = vec4(s.N * 0.5 + 0.5, 1.0);
            return true;
        case DEBUG_ROUGHNESS:
            outColor = vec4(vec3(s.roughness), 1.0);
            return true;
        case DEBUG_METALLIC:
            outColor = vec4(vec3(s.metallic), 1.0);
            return true;
        case DEBUG_AO:
            outColor = vec4(vec3(s.ao), 1.0);
            return true;
        case DEBUG_IBL_DIFFUSE:
            outColor = vec4(iblDiffuse, 1.0);
            return true;
        case DEBUG_IBL_SPECULAR:
            outColor = vec4(iblSpecular, 1.0);
            return true;
        case DEBUG_BRDF_LUT:
            outColor = vec4(vec3(texture(brdfLUT, vec2(s.NdotV, s.roughness)).x), 1.0);
            return true;

        // ---------------------------------------------------------------------
        // Cascaded Shadow Map (CSM) debug views (use helpers from shadows.glsl)
        // ---------------------------------------------------------------------
        case DEBUG_CSM_CASCADES:
            // Color-code cascade index at each pixel
            outColor = vec4(getCSMCascadeDebugColor(s.worldPos), 1.0);
            return true;

        case DEBUG_CSM_VIEW_DEPTH:
            // Visualize depth precision within the selected cascade
            outColor = vec4(getCSMDepthPrecisionDebug(s.worldPos), 1.0);
            return true;

        case DEBUG_CSM_SPLITS: {
            // Show which splits the view-depth exceeds (R= >split0, G=>split1, B=>split2)
            float vd     = getViewDepth(s.worldPos);
            vec3  splits = vec3(vd > ubo.directionalCascadeSplits.x ? 1.0 : 0.0,
                vd > ubo.directionalCascadeSplits.y ? 1.0 : 0.0,
                vd > ubo.directionalCascadeSplits.z ? 1.0 : 0.0);
            outColor = vec4(splits, 1.0);
            return true;
        }

        case DEBUG_CSM_DEPTH_HUE: {
            // Raw view-depth mapped to a simple blue->green->red ramp
            float vd  = getViewDepth(s.worldPos);
            float t   = clamp(vd / max(ubo.directionalCascadeSplits.w, 1.0), 0.0, 1.0);
            vec3  col = (t < 0.5) ? mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 0.0), t * 2.0) : mix(vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.5) * 2.0);
            outColor  = vec4(col, 1.0);
            return true;
        }

        case DEBUG_CSM_SAMPLES: {
            // Show per-cascade shadow sample (R= cascade0, G=cascade1, B=cascade2)
            // Also mask channels when the shader believes the corresponding shadow-map
            // binding is not present or the projected coords lie outside the cascade.
            vec3 samples  = vec3(1.0);
            vec3 validity = vec3(0.0);
            if (ubo.directionalLightCount > 0 && ubo.directionalCascadeCount > 0) {
                vec3 L = normalize(-directionalLights[0].direction.xyz);
                for (int c = 0; c < 3; ++c) {
                    if (c >= ubo.directionalCascadeCount)
                        continue;
                    int  lightIdx = ubo.directionalCascadeBaseIndex + c;
                    bool bound    = (lightIdx < ubo.shadowLightCount);
                    if (bound) {
                        // Quick proj-coords check (matches sampleShadowMapWithBias's early-out)
                        vec4 lsPos      = ubo.lightSpaceMatrices[lightIdx] * vec4(s.worldPos, 1.0);
                        vec3 projCoords = lsPos.xyz / lsPos.w;
                        projCoords.xy   = projCoords.xy * 0.5 + 0.5;
                        bool inRange    = !(projCoords.z < 0.0 || projCoords.z > 1.0 || any(lessThan(projCoords.xy, vec2(0.0))) || any(greaterThan(projCoords.xy, vec2(1.0))));
                        if (inRange)
                            validity[c] = 1.0;
                    }

                    // Still sample (returns 1.0 when unbound/outsided due to early-return) so
                    // we can compare sample vs validity in the debug view.
                    if (c == 0)
                        samples.r = sampleShadowMapWithBias(s.worldPos, s.N, L, lightIdx, 0, 1.0);
                    else if (c == 1)
                        samples.g = sampleShadowMapWithBias(s.worldPos, s.N, L, lightIdx, 1, 1.0);
                    else if (c == 2)
                        samples.b = sampleShadowMapWithBias(s.worldPos, s.N, L, lightIdx, 2, 1.0);
                }
            }

            // Visual encoding: channel = sample * validity
            outColor = vec4(samples * validity, 1.0);
            return true;
        }
    }
    return false;
}

/*==============================================================================
  Lighting calculations
==============================================================================*/

void calculateDirectLight(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0, vec3 L, vec3 radiance, out vec3 outDiffuse, out vec3 outSpecular) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    if (NdotL <= 0.0 || NdotV <= 0.0) {
        outDiffuse  = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    vec3  H   = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(NdotV, NdotL, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 1e-4;
    vec3  specular    = numerator / denominator;

    outDiffuse  = (kD * albedo / PI) * radiance * NdotL;
    outSpecular = specular * radiance * NdotL;
}

/*==============================================================================
  Directional
==============================================================================*/
vec3 handleDirectionalLights(in Surface s) {
    vec3 color = vec3(0.0);

    for (int i = 0; i < ubo.directionalLightCount; i++) {
        float intensity = directionalLights[i].color.w;
        if (intensity <= 1e-6)
            continue;

        vec3 L        = normalize(-directionalLights[i].direction.xyz);
        vec3 radiance = directionalLights[i].color.xyz * intensity;

        if (i == 0 && ubo.directionalCascadeCount > 0) {
            radiance *= calculateDirectionalCSMShadow(s.worldPos, s.N, L);
        }

        vec3 diff, spec;
        calculateDirectLight(s.N, s.V, s.albedo, s.metallic, s.roughness, s.F0, L, radiance, diff, spec);

        color += diff + spec;
    }

    return color;
}
/*==============================================================================
  Point
==============================================================================*/
vec3 handlePointLights(in Surface s) {
    vec3 color = vec3(0.0);

    for (int i = 0; i < ubo.pointLightCount; i++) {
        float intensity = pointLights[i].color.w;
        if (intensity <= 1e-6)
            continue;

        vec3  Lvec  = pointLights[i].position.xyz - s.worldPos;
        float dist2 = dot(Lvec, Lvec);

        float att = computeDistanceAttenuation(dist2, pointLights[i].radius2);
        if (att <= 0.0)
            continue;

        vec3  L      = normalize(Lvec);
        float shadow = calculatePointLightShadow(s.worldPos, i);

        vec3 radiance = pointLights[i].color.xyz * intensity * att * shadow;

        vec3 diff, spec;
        calculateDirectLight(s.N, s.V, s.albedo, s.metallic, s.roughness, s.F0, L, radiance, diff, spec);

        color += diff + spec;
    }

    return color;
}
/*==============================================================================
  Spot
==============================================================================*/
vec3 handleSpotLights(in Surface s) {
    vec3 color      = vec3(0.0);
    int  shadowBase = ubo.directionalCascadeCount;

    for (int i = 0; i < ubo.spotLightCount; i++) {
        float intensity = spotLights[i].color.w;
        if (intensity <= 1e-6)
            continue;

        vec3  toLight = spotLights[i].position.xyz - s.worldPos;
        float dist2   = dot(toLight, toLight);

        float att = computeDistanceAttenuation(dist2, spotLights[i].radius2);
        if (att <= 0.0)
            continue;

        vec3 L = normalize(toLight);

        vec3  lightDir = normalize(spotLights[i].direction.xyz);
        float theta    = dot(normalize(-toLight), lightDir);

        float cone = clamp((theta - spotLights[i].outerCutoff) / max(spotLights[i].direction.w - spotLights[i].outerCutoff, 1e-4), 0.0, 1.0);

        float shadow = calculateShadow(s.worldPos, s.N, L, shadowBase + i);

        vec3 radiance = spotLights[i].color.xyz * intensity * att * cone * shadow;

        vec3 diff, spec;
        calculateDirectLight(s.N, s.V, s.albedo, s.metallic, s.roughness, s.F0, L, radiance, diff, spec);

        color += diff + spec;
    }

    return color;
}
/*==============================================================================
  Main
==============================================================================*/

void main() {
    float depth = texture(depthMap, inUV).r;
    if (depth >= 1.0 - 1e-6) {
        outColor = vec4(0.0);
        return;
    }

    if (ubo.debugMode == DEBUG_DEPTH) {
        float near   = 0.1;
        float far    = 100.0;
        float linear = (2.0 * near) / (far + near - depth * (far - near));
        outColor     = vec4(vec3(linear), 1.0);
        return;
    }

    bool    isDebugPrimitive = false;
    Surface surf             = loadSurface(inUV, depth, isDebugPrimitive);

    if (isDebugPrimitive) {
        outColor = vec4(surf.albedo, 1.0);
        return;
    }

    LightingResult ibl = computeIBL(surf);

    if (handleDebug(surf, ibl.diffuse, ibl.specular))
        return;

    vec3 color = vec3(0.0);

    color += ibl.diffuse + ibl.specular;
    color += ubo.ambientLightColor.xyz * ubo.ambientLightColor.w * surf.albedo * surf.ao * 0.05;
    color += handleDirectionalLights(surf);
    color += handlePointLights(surf);
    color += handleSpotLights(surf);

    outColor = vec4(color, 1.0);
}
