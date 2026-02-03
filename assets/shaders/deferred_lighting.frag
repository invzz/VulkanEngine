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
// Note: Emissive is written directly to the HDR color buffer during the G-buffer pass.
// The deferred lighting pass loads the HDR color (LOAD_OP_LOAD) and uses additive blending,
// so emissive is already in the framebuffer. No need to sample it separately.
// Bindings 4-5 unused (kept for descriptor layout compatibility).

vec3 octDecode(vec2 e)
{
  // Input e expected in [-1,1]
  vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  if (v.z < 0.0)
  {
    v.xy = (1.0 - abs(v.yx)) * sign(v.xy);
  }
  return normalize(v);
}

float iorToF0(float ior)
{
  ior     = max(ior, 1e-3);
  float a = (ior - 1.0) / (ior + 1.0);
  return a * a;
}

// IBL textures (irradiance, prefilter, BRDF LUT)
// IBL lives at set = 3 (matches LightingRenderBindings::kIBLSetIndex)
layout(set = 3, binding = 0) uniform samplerCube irradianceMap;
layout(set = 3, binding = 1) uniform samplerCube prefilterMap;
layout(set = 3, binding = 2) uniform sampler2D brdfLUT;

// Shadows
// Shadows live at set = 2 (matches LightingRenderBindings::kShadowSetIndex)
layout(set = 2, binding = 0) uniform sampler2DShadow shadowMaps[8];
layout(set = 2, binding = 1) uniform samplerCube cubeShadowMaps[4];

#include "includes/shadows.glsl"

vec3 reconstructWorldPos(vec2 uv, float depth)
{
  vec4 clip    = vec4(uv * 2.0 - 1.0, depth, 1.0);
  mat4 invProj = inverse(ubo.proj);
  mat4 invView = inverse(ubo.view);

  vec4 viewPos = invProj * clip;
  viewPos /= max(viewPos.w, 1e-6);
  vec4 worldPos = invView * viewPos;
  return worldPos.xyz;
}

void calculateDirectLight(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0, vec3 L, vec3 radiance, out vec3 outDiffuse, out vec3 outSpecular)
{
  float NdotL = max(dot(N, L), 0.0);
  float NdotV = max(dot(N, V), 0.0);

  if (NdotL <= 0.0 || NdotV <= 0.0)
  {
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

vec3 applyIridescenceToF0(vec3 F0, float metallic, float NdotV, float iridescenceFactor, float iridescenceIOR, float iridescenceThicknessNm)
{
  float strength = clamp(iridescenceFactor, 0.0, 1.0);
  if (strength <= 1e-4)
  {
    return F0;
  }

  float ior       = max(iridescenceIOR, 1.0);
  float thickness = max(iridescenceThicknessNm, 0.0);
  float cosThetaI = clamp(NdotV, 0.0, 1.0);

  // Approximate transmitted angle through the thin film (air -> film).
  // This makes IOR affect phase more strongly than using NdotV directly.
  float sinThetaI2 = max(1.0 - cosThetaI * cosThetaI, 0.0);
  float sinThetaT2 = sinThetaI2 / max(ior * ior, 1e-6);
  float cosThetaT  = sqrt(max(1.0 - sinThetaT2, 0.0));

  // Lightweight thin-film interference approximation (nm wavelengths).
  // NOTE: This is not a full Fresnel+multi-bounce film model, but it produces
  // a clear iridescent shift on dielectrics while remaining stable.
  vec3 lambda       = vec3(650.0, 510.0, 475.0);
  vec3 phase        = 4.0 * PI * ior * thickness * cosThetaT / lambda;
  vec3 interference = clamp(0.5 + 0.5 * cos(phase), 0.0, 1.0);

  // Keep overall energy stable: apply chroma around a base.
  float avg    = dot(interference, vec3(1.0 / 3.0));
  vec3  chroma = interference - vec3(avg);

  // Dielectrics: use plausible film interface reflectance as base.
  // Metals: tint the existing F0 so we don't erase metallic albedo-driven reflectance.
  float f0Film   = pow((ior - 1.0) / (ior + 1.0), 2.0);
  vec3  baseF0   = mix(vec3(f0Film), F0, step(0.5, metallic));
  vec3  targetF0 = clamp(baseF0 + chroma * 0.35, 0.0, 1.0);

  return mix(F0, targetF0, strength);
}

void main()
{
  float depth = texture(depthMap, inUV).r;
  if (depth >= 1.0 - 1e-6)
  {
    // Sky / background (depth at far plane): contribute nothing.
    outColor = vec4(0.0);
    return;
  }

  // Debug mode 9: Depth visualization
  if (ubo.debugMode == 9)
  {
    // Linearize depth for better visualization
    float near        = 0.1;
    float far         = 100.0;
    float linearDepth = (2.0 * near) / (far + near - depth * (far - near));
    outColor          = vec4(vec3(linearDepth), 1.0);
    return;
  }

  // Debug mode 6: Emissive only
  // Note: Emissive is in the framebuffer (loaded via LOAD_OP_LOAD), not a separate sampler.
  // This debug mode cannot display emissive since we don't sample it anymore.
  // Kept for compatibility; shows black.
  if (ubo.debugMode == 6)
  {
    outColor = vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }

  vec3 worldPos = reconstructWorldPos(inUV, depth);
  vec3 V        = normalize(ubo.cameraPosition.xyz - worldPos);

  vec4 nPacked = texture(gbufferNormal, inUV);
  // G-buffer normal packing:
  // nrm.rg: oct-encoded world normal (in [0,1])
  // nrm.b : material IOR
  // nrm.a : iridescence (thin film) IOR
  vec2  nOct           = nPacked.rg * 2.0 - 1.0;
  vec3  N              = octDecode(nOct);
  float materialIOR    = nPacked.b;
  float iridescenceIOR = nPacked.a;

  float NdotV = max(dot(N, V), 0.0);

  vec4  albedoA              = texture(gbufferAlbedo, inUV);
  vec3  albedo               = albedoA.rgb;
  float iridescenceThickness = albedoA.a;

  // Debug modes 7 (Meshlets) and 8 (Meshlet Cones) use negative alpha as flag
  // The debug color is stored in albedo by gbuffer.frag
  if (iridescenceThickness < 0.0)
  {
    outColor = vec4(albedo, 1.0);
    return;
  }

  vec4  matRMao           = texture(gbufferMaterial, inUV);
  float metallic          = matRMao.r;
  float roughness         = matRMao.g;
  float ao                = matRMao.b;
  float iridescenceFactor = matRMao.a;

  // Debug views (match GlobalUbo::debugMode conventions used elsewhere)
  // 1: Albedo, 2: Normal, 3: Roughness, 4: Metallic, 5: Lighting
  if (ubo.debugMode == 1)
  {
    outColor = vec4(albedo, 1.0);
    return;
  }
  if (ubo.debugMode == 2)
  {
    outColor = vec4(N * 0.5 + 0.5, 1.0);
    return;
  }
  if (ubo.debugMode == 3)
  {
    outColor = vec4(vec3(roughness), 1.0);
    return;
  }
  if (ubo.debugMode == 4)
  {
    outColor = vec4(vec3(metallic), 1.0);
    return;
  }

  // Debug mode 10: Ambient Occlusion visualization
  if (ubo.debugMode == 10)
  {
    outColor = vec4(vec3(ao), 1.0);
    return;
  }

  float dielectricF0 = iorToF0(materialIOR);
  vec3  F0           = mix(vec3(dielectricF0), albedo, metallic);
  F0                 = applyIridescenceToF0(F0, metallic, NdotV, iridescenceFactor, iridescenceIOR, iridescenceThickness);
  vec3 R             = reflect(-V, N);

  // IBL (diffuse + specular reflection)
  vec3 F_roughness = fresnelSchlickRoughness(NdotV, F0, roughness);
  vec3 kS          = F_roughness;
  vec3 kD          = (vec3(1.0) - kS) * (1.0 - metallic);

  vec3 irradiance = texture(irradianceMap, N).rgb;
  vec3 diffuseIBL = kD * irradiance * albedo * ao;

  vec3  prefilteredColor = textureLod(prefilterMap, R, roughness * kMaxReflectionLod).rgb;
  vec2  brdf             = texture(brdfLUT, vec2(NdotV, roughness)).rg;
  vec3  specularIBL      = prefilteredColor * (F0 * brdf.x + brdf.y);
  float horizon          = min(1.0 + dot(R, N), 1.0);
  specularIBL *= horizon * horizon;
  specularIBL *= computeSpecularAO(NdotV, ao, roughness);

  // Debug mode 12: IBL diffuse (irradiance) visualization
  if (ubo.debugMode == 12)
  {
    vec3 irrDbg = texture(irradianceMap, N).rgb;
    outColor    = vec4(irrDbg, 1.0);
    return;
  }

  // Debug mode 13: IBL specular prefilter visualization
  if (ubo.debugMode == 13)
  {
    vec3 prefilterDbg = textureLod(prefilterMap, R, roughness * kMaxReflectionLod).rgb;
    outColor          = vec4(prefilterDbg, 1.0);
    return;
  }

  // Debug mode 14: BRDF LUT sample visualization
  if (ubo.debugMode == 14)
  {
    vec2 brdfDbg = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    outColor     = vec4(vec3(brdfDbg.x), 1.0);
    return;
  }

  // Debug mode 15: CSM cascade visualization (Red=0, Green=1, Blue=2, Yellow=3)
  if (ubo.debugMode == 15)
  {
    int cascade = getCSMCascadeIndex(worldPos);
    vec3 cascadeColors[4] = vec3[4](
      vec3(1.0, 0.0, 0.0),  // Cascade 0: Red
      vec3(0.0, 1.0, 0.0),  // Cascade 1: Green
      vec3(0.0, 0.0, 1.0),  // Cascade 2: Blue
      vec3(1.0, 1.0, 0.0)   // Cascade 3: Yellow
    );
    vec3 color = (cascade >= 0 && cascade < 4) ? cascadeColors[cascade] : vec3(1.0);
    outColor = vec4(color, 1.0);
    return;
  }

  // Debug mode 16: View-space depth visualization
  if (ubo.debugMode == 16)
  {
    float depth = getCSMViewDepth(worldPos);
    // Normalize to [0,1] range using splits.w (last cascade split) as max
    float maxDist = max(ubo.directionalCascadeSplits.w, 1.0);
    float normalized = clamp(depth / maxDist, 0.0, 1.0);
    outColor = vec4(vec3(normalized), 1.0);
    return;
  }

  // Ambient fallback boost (kept small; matches forward path behavior)
  diffuseIBL += ubo.ambientLightColor.xyz * ubo.ambientLightColor.w * albedo * ao * 0.05;

  vec3 outLit = diffuseIBL + specularIBL;

  // Debug mode 5: Lighting only (pre-baked/emissive)
  if (ubo.debugMode == 5)
  {
    outColor = vec4(outLit, 1.0);
    return;
  }

  // Directional lights (diffuse + specular)
  for (int i = 0; i < ubo.directionalLightCount; i++)
  {
    float intensity = directionalLights[i].color.w;
    if (intensity <= 1e-6) continue;

    vec3 L        = normalize(-directionalLights[i].direction.xyz);
    vec3 radiance = directionalLights[i].color.xyz * intensity;

    float shadow = 1.0;
    // CSM: the first directional light uses cascades starting at ubo.directionalCascadeBaseIndex.
    if (i == 0 && ubo.directionalCascadeCount > 0)
    {
      shadow = calculateDirectionalCSMShadow(worldPos, N, L);
    }
    radiance *= shadow;

    vec3 diff, spec;
    calculateDirectLight(N, V, albedo, metallic, roughness, F0, L, radiance, diff, spec);
    outLit += diff + spec;
  }

  // Point lights (diffuse + specular)
  for (int i = 0; i < ubo.pointLightCount; i++)
  {
    float intensity = pointLights[i].color.w;
    if (intensity <= 1e-6) continue;

    vec3  lightPos = pointLights[i].position.xyz;
    vec3  Lvec     = lightPos - worldPos;
    float dist2    = dot(Lvec, Lvec);
    float radius2  = max(pointLights[i].radius2, 0.0);
    if (radius2 > 0.0 && dist2 > radius2) continue;

    float dist = sqrt(max(dist2, 1e-6));
    vec3  L    = Lvec / dist;

    // Smooth radius falloff + inverse-square.
    float fade        = (radius2 > 0.0) ? clamp(1.0 - dist2 / radius2, 0.0, 1.0) : 1.0;
    float attenuation = (fade * fade) / max(dist2, 1e-4);

    float shadow = calculatePointLightShadow(worldPos, i);

    vec3 radiance = pointLights[i].color.xyz * intensity * attenuation * shadow;

    vec3 diff, spec;
    calculateDirectLight(N, V, albedo, metallic, roughness, F0, L, radiance, diff, spec);
    outLit += diff + spec;
  }

  // Spot lights (diffuse + specular)
  int shadowBase = ubo.directionalCascadeCount;
  for (int i = 0; i < ubo.spotLightCount; i++)
  {
    float intensity = spotLights[i].color.w;
    if (intensity <= 1e-6) continue;

    vec3 lightPos = spotLights[i].position.xyz;
    vec3 lightDir = normalize(spotLights[i].direction.xyz);

    vec3  toLight = lightPos - worldPos;
    float dist2   = dot(toLight, toLight);
    float radius2 = max(spotLights[i].radius2, 0.0);
    if (radius2 > 0.0 && dist2 > radius2) continue;

    float dist = sqrt(max(dist2, 1e-6));
    vec3  L    = toLight / dist;

    // Cone attenuation (cosine space).
    vec3  lightToFragDir = normalize(worldPos - lightPos);
    float theta          = dot(lightToFragDir, lightDir);
    float outerCutoff    = spotLights[i].outerCutoff;
    float innerCutoff    = spotLights[i].direction.w;
    float cone           = clamp((theta - outerCutoff) / max(innerCutoff - outerCutoff, 1e-4), 0.0, 1.0);

    float attenuation = 1.0 / max(spotLights[i].constantAtten + spotLights[i].linearAtten * dist + spotLights[i].quadraticAtten * dist2, 1e-4);

    float shadow = calculateShadow(worldPos, N, L, shadowBase + i);

    vec3 radiance = spotLights[i].color.xyz * intensity * attenuation * cone * shadow;

    vec3 diff, spec;
    calculateDirectLight(N, V, albedo, metallic, roughness, F0, L, radiance, diff, spec);
    outLit += diff + spec;
  }

  outColor = vec4(outLit, 1.0);
}
