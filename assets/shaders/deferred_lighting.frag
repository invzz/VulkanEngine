#version 460
#if RAY_TRACING_ENABLED
#extension GL_EXT_ray_query : enable
#endif
#extension GL_GOOGLE_include_directive : enable

#include "includes/brdf.glsl"
#include "includes/scene_ubo.glsl"

/*==============================================================================
  Constants
==============================================================================*/

#define MAX_REFLECTION_LOD 4.0

/* Debug modes */
const int DEBUG_ALBEDO        = 1;
const int DEBUG_NORMAL        = 2;
const int DEBUG_ROUGHNESS     = 3;
const int DEBUG_METALLIC      = 4;
const int DEBUG_LIGHTING_ONLY = 5;
const int DEBUG_EMISSIVE_ONLY = 6;
const int DEBUG_MESHLETS      = 7;
const int DEBUG_MESHLET_CONES = 8;
const int DEBUG_DEPTH         = 9;
const int DEBUG_AO            = 10;
const int DEBUG_IBL_DIFFUSE   = 12;
const int DEBUG_IBL_SPECULAR  = 13;
const int DEBUG_BRDF_LUT      = 14;

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
layout(set = 2, binding = 0) uniform sampler2DShadow shadowMaps[4];
layout(set = 2, binding = 1) uniform samplerCube cubeShadowMaps[4];

#if RAY_TRACING_ENABLED
/* Ray tracing TLAS (global set, binding 2) */
layout(set = 0, binding = 2) uniform accelerationStructureEXT tlas;
#endif

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
    Surface     s;
    const float minMaterialIOR        = 1.0;
    const float materialIORRange      = 1.5;
    const float defaultIridescenceIOR = 1.3;

    s.worldPos = reconstructWorldPos(uv, depth);
    s.V        = normalize(ubo.cameraPosition.xyz - s.worldPos);

    vec4 nPacked = texture(gbufferNormal, uv);
    s.N          = octDecode(nPacked.rg);

    vec4 albedoA = texture(gbufferAlbedo, uv);
    s.albedo     = albedoA.rgb;

    float iridescenceThickness = albedoA.a;

    isDebugPrimitive = (iridescenceThickness < 0.0);

    vec4 mat    = texture(gbufferMaterial, uv);
    s.metallic  = mat.r;
    s.roughness = mat.g;
    s.ao        = mat.b;

    uint packedByte = uint(round(clamp(mat.a, 0.0, 1.0) * 255.0));
    uint iorQ       = (packedByte >> 4u) & 0xFu;
    uint iridesQ    = packedByte & 0xFu;

    float materialIOR = minMaterialIOR + (float(iorQ) / 15.0) * materialIORRange;
    float iridescence = float(iridesQ) / 15.0;

    s.NdotV = max(dot(s.N, s.V), 0.0);

    float dielectricF0 = iorToF0(materialIOR);
    s.F0               = mix(vec3(dielectricF0), s.albedo, s.metallic);

    s.F0 = applyIridescenceToF0(s.F0, s.metallic, s.NdotV, iridescence, defaultIridescenceIOR, iridescenceThickness);

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
    // Apply Burley-like roughness scaling to IBL diffuse for consistency with direct lighting
    float fdIBL = 1.0 + s.roughness * s.roughness * 0.5;
    r.diffuse       = kD * irradiance * s.albedo * s.ao * fdIBL;

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
    float NdotH = max(dot(N, H), 0.0);
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(NdotV, NdotL, roughness);
    vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 1e-4;
    vec3  specular    = numerator / denominator;

    // Burley (Disney) diffuse — more realistic than Lambert for rough surfaces
    float fd = burleyDiffuse(NdotL, NdotV, NdotH, roughness);
    outDiffuse  = (kD * albedo / PI) * radiance * NdotL * fd;
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

        vec3 Lraw = -directionalLights[i].direction.xyz;
        if (dot(Lraw, Lraw) < 1e-8) {
            Lraw = vec3(0.0, 1.0, 0.0);
        }
        vec3 L        = normalize(Lraw);
        vec3 radiance = directionalLights[i].color.xyz * intensity;

        vec3 diff, spec;
        calculateDirectLight(s.N, s.V, s.albedo, s.metallic, s.roughness, s.F0, L, radiance, diff, spec);

#if RAY_TRACING_ENABLED
        // Ray-traced shadow — bias-free, no acne, no Peter Panning
        float shadow = 1.0;
        {
            vec3  origin = s.worldPos + s.N * 0.002;
            rayQueryEXT q;
            rayQueryInitializeEXT(q, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, origin, 0.0, L, 1000.0);
            rayQueryProceedEXT(q);
            bool hit = rayQueryGetIntersectionTypeEXT(q, true) != gl_RayQueryCommittedIntersectionNoneEXT;
            shadow = hit ? 0.0 : 1.0;
        }
#else
        float shadow = calculateCascadeShadow(s.worldPos, s.N, L);
#endif
        color += (diff + spec) * shadow;
    }

    return color;
}
/*==============================================================================
  Point
==============================================================================*/
vec3 handlePointLights(in Surface s) {
    vec3 color = vec3(0.0);

    for (int i = 0; i < ubo.pointLightCount; i++) {
        float intensity = pointLights[i].colorIntensity.w;
        if (intensity <= 1e-6)
            continue;

        vec3  Lvec  = pointLights[i].positionRadius2.xyz - s.worldPos;
        float dist2 = dot(Lvec, Lvec);

        float att = computeDistanceAttenuation(dist2, pointLights[i].positionRadius2.w);
        if (att <= 0.0)
            continue;

        vec3  L      = normalize(Lvec);
        float shadow = calculatePointLightShadow(s.worldPos, i);

        vec3 radiance = pointLights[i].colorIntensity.xyz * intensity * att * shadow;

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
    int  shadowBase = 0;

    for (int i = 0; i < ubo.spotLightCount; i++) {
        float intensity = spotLights[i].colorIntensity.w;
        if (intensity <= 1e-6)
            continue;

        vec3  toLight = spotLights[i].positionRadius2.xyz - s.worldPos;
        float dist2   = dot(toLight, toLight);

        float att = computeDistanceAttenuation(dist2, spotLights[i].positionRadius2.w);
        if (att <= 0.0)
            continue;

        vec3 L = normalize(toLight);

        vec3  lightDir = normalize(spotLights[i].directionInner.xyz);
        float theta    = dot(normalize(-toLight), lightDir);

        float cone = clamp((theta - spotLights[i].attenOuter.x) / max(spotLights[i].directionInner.w - spotLights[i].attenOuter.x, 1e-4), 0.0, 1.0);

        float shadow = calculateShadow(s.worldPos, s.N, L, shadowBase + i);

        vec3 radiance = spotLights[i].colorIntensity.xyz * intensity * att * cone * shadow;

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
