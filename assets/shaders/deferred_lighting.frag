#version 450
#extension GL_GOOGLE_include_directive : enable

#include "includes/brdf.glsl"
#include "includes/scene_ubo.glsl"

const float kMaxReflectionLod = 4.0;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D gbufferNormal;
layout(set = 1, binding = 1) uniform sampler2D gbufferAlbedo;
layout(set = 1, binding = 2) uniform sampler2D gbufferMaterial;
layout(set = 1, binding = 3) uniform sampler2D depthMap;

// IBL textures (irradiance, prefilter, BRDF LUT)
layout(set = 2, binding = 0) uniform samplerCube irradianceMap;
layout(set = 2, binding = 1) uniform samplerCube prefilterMap;
layout(set = 2, binding = 2) uniform sampler2D brdfLUT;

// Shadows
layout(set = 3, binding = 0) uniform sampler2DShadow shadowMaps[4];
layout(set = 3, binding = 1) uniform samplerCube cubeShadowMaps[4];

#include "includes/shadows.glsl"

vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    mat4 invProj = inverse(ubo.proj);
    mat4 invView = inverse(ubo.view);

    vec4 viewPos = invProj * clip;
    viewPos /= max(viewPos.w, 1e-6);
    vec4 worldPos = invView * viewPos;
    return worldPos.xyz;
}

void calculateDirectLight(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0, vec3 L, vec3 radiance, out vec3 outDiffuse, out vec3 outSpecular) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    if (NdotL <= 0.0 || NdotV <= 0.0) {
        outDiffuse  = vec3(0.0);
        outSpecular = vec3(0.0);
        return;
    }

    vec3 H = normalize(V + L);
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

vec3 applyIridescenceToF0(vec3 F0, float metallic, float NdotV, float iridescenceFactor, float iridescenceIOR, float iridescenceThicknessNm) {
    float strength = clamp(iridescenceFactor, 0.0, 1.0) * (1.0 - metallic);
    if (strength <= 1e-4) {
        return F0;
    }

    float ior = max(iridescenceIOR, 1.0);
    float thickness = max(iridescenceThicknessNm, 0.0);
    float cosTheta = clamp(NdotV, 0.0, 1.0);

    // Very lightweight thin-film interference tint approximation (nm wavelengths).
    vec3 lambda = vec3(650.0, 510.0, 475.0);
    vec3 phase = 4.0 * PI * ior * thickness * cosTheta / lambda;
    vec3 interference = 0.5 + 0.5 * cos(phase);

    vec3 hue = normalize(interference + vec3(1e-3));
    vec3 tinted = clamp(F0 * hue * 3.0, 0.0, 1.0);
    return mix(F0, tinted, strength);
}

void main() {
    vec4 nPacked = texture(gbufferNormal, inUV);
    vec3 N = nPacked.xyz;
    float nLen = length(N);
    if (nLen > 1e-6) {
        N /= nLen;
    } else {
        // No geometry written to G-buffer here; contribute nothing (additive pass).
        outColor = vec4(0.0);
        return;
    }

    float depth = texture(depthMap, inUV).r;
    if (depth >= 1.0 - 1e-6) {
        // Sky / background (depth at far plane): contribute nothing.
        outColor = vec4(0.0);
        return;
    }

    vec3 worldPos = reconstructWorldPos(inUV, depth);
    vec3 V = normalize(ubo.cameraPosition.xyz - worldPos);
    float NdotV = max(dot(N, V), 0.0);

    vec4 albedoA = texture(gbufferAlbedo, inUV);
    vec3 albedo  = albedoA.rgb;
    float iridescenceThickness = albedoA.a;

    vec4 matRMao = texture(gbufferMaterial, inUV);
    float metallic  = matRMao.r;
    float roughness = matRMao.g;
    float ao        = matRMao.b;
    float iridescenceFactor = matRMao.a;
    float iridescenceIOR    = nPacked.a;

    // Debug views (match GlobalUbo::debugMode conventions used elsewhere)
    // 1: Albedo, 2: Normal, 3: Roughness, 4: Metallic, 5: Lighting
    if (ubo.debugMode == 1) {
        outColor = vec4(albedo, 1.0);
        return;
    }
    if (ubo.debugMode == 2) {
        outColor = vec4(N * 0.5 + 0.5, 1.0);
        return;
    }
    if (ubo.debugMode == 3) {
        outColor = vec4(vec3(roughness), 1.0);
        return;
    }
    if (ubo.debugMode == 4) {
        outColor = vec4(vec3(metallic), 1.0);
        return;
    }

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    F0 = applyIridescenceToF0(F0, metallic, NdotV, iridescenceFactor, iridescenceIOR, iridescenceThickness);
    vec3 R  = reflect(-V, N);

    // IBL (diffuse + specular reflection)
    vec3 F_roughness = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F_roughness;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = kD * irradiance * albedo * ao;

    vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * kMaxReflectionLod).rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);
    float horizon = min(1.0 + dot(R, N), 1.0);
    specularIBL *= horizon * horizon;
    specularIBL *= computeSpecularAO(NdotV, ao, roughness);

    // Ambient fallback boost (kept small; matches forward path behavior)
    diffuseIBL += ubo.ambientLightColor.xyz * ubo.ambientLightColor.w * albedo * ao * 0.05;

    vec3 outLit = diffuseIBL + specularIBL;

    // Directional lights (diffuse + specular)
    for (int i = 0; i < ubo.directionalLightCount; i++) {
        float intensity = ubo.directionalLights[i].color.w;
        if (intensity <= 1e-6) continue;

        vec3 L = normalize(-ubo.directionalLights[i].direction.xyz);
        vec3 radiance = ubo.directionalLights[i].color.xyz * intensity;

        float shadow = 1.0;
        // ShadowSystem renders at most 1 directional shadow, in index 0.
        if (i == 0 && ubo.shadowLightCount > 0) {
            shadow = calculateShadow(worldPos, 0);
        }
        radiance *= shadow;

        vec3 diff, spec;
        calculateDirectLight(N, V, albedo, metallic, roughness, F0, L, radiance, diff, spec);
        outLit += diff + spec;
    }

    // Point lights (diffuse + specular)
    for (int i = 0; i < ubo.pointLightCount; i++) {
        float intensity = ubo.pointLights[i].color.w;
        if (intensity <= 1e-6) continue;

        vec3 lightPos = ubo.pointLights[i].position.xyz;
        vec3 Lvec = lightPos - worldPos;
        float dist2 = dot(Lvec, Lvec);
        float radius2 = max(ubo.pointLights[i].radius2, 0.0);
        if (radius2 > 0.0 && dist2 > radius2) continue;

        float dist = sqrt(max(dist2, 1e-6));
        vec3 L = Lvec / dist;

        // Smooth radius falloff + inverse-square.
        float fade = (radius2 > 0.0) ? clamp(1.0 - dist2 / radius2, 0.0, 1.0) : 1.0;
        float attenuation = (fade * fade) / max(dist2, 1e-4);

        float shadow = calculatePointLightShadow(worldPos, i);

        vec3 radiance = ubo.pointLights[i].color.xyz * intensity * attenuation * shadow;

        vec3 diff, spec;
        calculateDirectLight(N, V, albedo, metallic, roughness, F0, L, radiance, diff, spec);
        outLit += diff + spec;
    }

    // Spot lights (diffuse + specular)
    int shadowBase = (ubo.directionalLightCount > 0) ? 1 : 0;
    for (int i = 0; i < ubo.spotLightCount; i++) {
        float intensity = ubo.spotLights[i].color.w;
        if (intensity <= 1e-6) continue;

        vec3 lightPos = ubo.spotLights[i].position.xyz;
        vec3 lightDir = normalize(ubo.spotLights[i].direction.xyz);

        vec3 toLight = lightPos - worldPos;
        float dist2 = dot(toLight, toLight);
        float radius2 = max(ubo.spotLights[i].radius2, 0.0);
        if (radius2 > 0.0 && dist2 > radius2) continue;

        float dist = sqrt(max(dist2, 1e-6));
        vec3 L = toLight / dist;

        // Cone attenuation (cosine space).
        vec3 lightToFragDir = normalize(worldPos - lightPos);
        float theta = dot(lightToFragDir, lightDir);
        float outerCutoff = ubo.spotLights[i].outerCutoff;
        float innerCutoff = ubo.spotLights[i].direction.w;
        float cone = clamp((theta - outerCutoff) / max(innerCutoff - outerCutoff, 1e-4), 0.0, 1.0);

        float attenuation = 1.0 / max(
            ubo.spotLights[i].constantAtten + ubo.spotLights[i].linearAtten * dist + ubo.spotLights[i].quadraticAtten * dist2,
            1e-4);

        float shadow = calculateShadow(worldPos, shadowBase + i);

        vec3 radiance = ubo.spotLights[i].color.xyz * intensity * attenuation * cone * shadow;

        vec3 diff, spec;
        calculateDirectLight(N, V, albedo, metallic, roughness, F0, L, radiance, diff, spec);
        outLit += diff + spec;
    }

    outColor = vec4(outLit, 1.0);
}
