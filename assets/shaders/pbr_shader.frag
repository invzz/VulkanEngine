#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragmentWorldPos;
layout(location = 2) in vec3 fragmentNormalWorld;
layout(location = 3) in vec2 fragUV;

struct PointLight
{
  vec4 position;
  vec4 color;
};

struct DirectionalLight
{
  vec4 direction;
  vec4 color;
};

struct SpotLight
{
  vec4  position;
  vec4  direction;   // w component is inner cutoff (cos)
  vec4  color;       // w component is intensity
  float outerCutoff; // Outer cutoff (cos)
  float constantAtten;
  float linearAtten;
  float quadraticAtten;
};

layout(set = 0, binding = 0) uniform UBO
{
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
  int              _pad1;
  int              _pad2;
  int              _pad3;
  vec4             frustumPlanes[6];
}
ubo;

// Global textures (set 1)
layout(set = 1, binding = 0) uniform sampler2D globalTextures[];

// Shadow maps (set 2) - array of shadow maps for multiple lights
layout(set = 2, binding = 0) uniform sampler2DShadow shadowMaps[4];

// Cube shadow maps for point lights (set 2, binding 1)
layout(set = 2, binding = 1) uniform samplerCube cubeShadowMaps[4];

// IBL textures (set 3)
layout(set = 3, binding = 0) uniform samplerCube irradianceMap;
layout(set = 3, binding = 1) uniform samplerCube prefilterMap;
layout(set = 3, binding = 2) uniform sampler2D brdfLUT;

// Push constants: Only material portion visible in fragment shader (offset 128)
layout(set = 4, binding = 0) uniform MaterialData
{
  vec4  albedo;
  float metallic;
  float roughness;
  float ao;
  float isSelected;
  float clearcoat;
  float clearcoatRoughness;
  float anisotropic;
  float anisotropicRotation;
  float transmission;
  float ior;
  float iridescence;
  float iridescenceIOR;
  float iridescenceThickness;
  uint  textureFlags;
  float uvScale;
  float alphaCutoff;
  uint  alphaMode;
  uint  albedoIndex;
  uint  normalIndex;
  uint  metallicIndex;
  uint  roughnessIndex;
  uint  aoIndex;
  uint  emissiveIndex;
  uint  _padding;
  vec4  emissiveInfo; // rgb: color, a: strength
}
material;

const float PI = 3.14159265359;

// GGX/Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
  float a      = roughness * roughness;
  float a2     = a * a;
  float NdotH  = max(dot(N, H), 0.0);
  float NdotH2 = NdotH * NdotH;

  float num   = a2;
  float denom = (NdotH2 * (a2 - 1.0) + 1.0);
  denom       = PI * denom * denom;

  return num / denom;
}

// Smith's Geometry Shadowing Function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness)
{
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float num   = NdotV;
  float denom = NdotV * (1.0 - k) + k;

  return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
  float NdotV = max(dot(N, V), 0.0);
  float NdotL = max(dot(N, L), 0.0);
  float ggx2  = GeometrySchlickGGX(NdotV, roughness);
  float ggx1  = GeometrySchlickGGX(NdotL, roughness);

  return ggx1 * ggx2;
}

// Fresnel-Schlick Approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
  return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
  return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Calculate shadow factor using PCF (Percentage Closer Filtering)
float calculateShadow(vec3 worldPos, int lightIndex)
{
  if (lightIndex >= ubo.shadowLightCount) return 1.0;

  // Transform world position to light space
  vec4 lightSpacePos = ubo.lightSpaceMatrices[lightIndex] * vec4(worldPos, 1.0);

  // Perspective divide (needed for spotlight perspective projection)
  vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

  // Transform from [-1,1] to [0,1] for UV lookup (Vulkan already has Z in [0,1])
  projCoords.xy = projCoords.xy * 0.5 + 0.5;

  // Check if outside shadow map bounds
  if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z < 0.0 || projCoords.z > 1.0)
  {
    return 1.0; // No shadow outside light frustum
  }

  // PCF 3x3 sampling for soft shadows
  float shadow    = 0.0;
  vec2  texelSize = 1.0 / textureSize(shadowMaps[lightIndex], 0);

  for (int x = -1; x <= 1; x++)
  {
    for (int y = -1; y <= 1; y++)
    {
      vec2 offset = vec2(x, y) * texelSize;
      // sampler2DShadow returns 0 or 1 based on depth comparison
      shadow += texture(shadowMaps[lightIndex], vec3(projCoords.xy + offset, projCoords.z));
    }
  }
  shadow /= 9.0;

  return shadow;
}

// Calculate shadow factor for point light using cube shadow map
float calculatePointLightShadow(vec3 worldPos, int lightIndex)
{
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
  float shadow = (normalizedDepth > closestDepth + bias) ? 0.0 : 1.0;

  return shadow;
}

// Anisotropic GGX Distribution
float DistributionGGXAnisotropic(vec3 N, vec3 H, vec3 T, vec3 B, float roughness, float anisotropy)
{
  float at = max(roughness * (1.0 + anisotropy), 0.001);
  float ab = max(roughness * (1.0 - anisotropy), 0.001);

  float ToH = dot(T, H);
  float BoH = dot(B, H);
  float NoH = dot(N, H);

  float a2 = at * ab;
  vec3  v  = vec3(ab * ToH, at * BoH, a2 * NoH);
  float v2 = dot(v, v);
  float w2 = a2 / v2;

  return a2 * w2 * w2 / PI;
}

vec3 evalIridescence(float NdotV, float thickness, float ior)
{
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

void main()
{
  // Apply UV tiling scale
  vec2 uv = fragUV * material.uvScale;

  // Sample material properties from textures or use push constants as base values
  vec4  baseColor = material.albedo;
  vec3  albedo    = baseColor.rgb;
  float alpha     = baseColor.a;
  float metallic  = material.metallic;
  float roughness = material.roughness;
  float ao        = material.ao;

  // Texture sampling: Check flags to determine which textures are bound
  // Push constant values act as multipliers/fallbacks when textures are present

  if ((material.textureFlags & (1u << 0)) != 0u) // Albedo/BaseColor texture
  {
    vec4 texColor = texture(globalTextures[nonuniformEXT(material.albedoIndex)], uv);
    albedo        = texColor.rgb; // sRGB texture, auto-converted to linear by GPU
    alpha         = texColor.a;
  }

  // Alpha Masking
  if (material.alphaMode == 1) // MASK
  {
    if (alpha < material.alphaCutoff)
    {
      discard;
    }
    alpha = 1.0; // Treat as opaque after test
  }
  else if (material.alphaMode == 0) // OPAQUE
  {
    alpha = 1.0;
  }

  bool aoHandled = false;

  if ((material.textureFlags & (1u << 7)) != 0u) // OcclusionRoughnessMetallic Packed
  {
    // glTF: Red = Occlusion, Green = Roughness, Blue = Metallic
    vec4 ormSample = texture(globalTextures[nonuniformEXT(material.roughnessIndex)], uv);
    ao             = ormSample.r;
    roughness      = ormSample.g;
    metallic       = ormSample.b;
    aoHandled      = true;
  }
  else if ((material.textureFlags & (1u << 6)) != 0u) // MetallicRoughness Packed (glTF)
  {
    // glTF: Blue = Metallic, Green = Roughness
    // We assume the texture is bound to roughnessIndex
    vec4 mrSample = texture(globalTextures[nonuniformEXT(material.roughnessIndex)], uv);
    metallic *= mrSample.b;
    roughness *= mrSample.g;
  }
  else
  {
    if ((material.textureFlags & (1u << 2)) != 0u) // Metallic texture
    {
      metallic *= texture(globalTextures[nonuniformEXT(material.metallicIndex)], uv).r; // Multiply push constant by texture value
    }

    if ((material.textureFlags & (1u << 3)) != 0u) // Roughness texture
    {
      roughness *= texture(globalTextures[nonuniformEXT(material.roughnessIndex)], uv).r;
    }
  }

  if (!aoHandled && (material.textureFlags & (1u << 4)) != 0u) // Ambient Occlusion texture
  {
    ao *= texture(globalTextures[nonuniformEXT(material.aoIndex)], uv).r;
  }

  vec3 N = normalize(fragmentNormalWorld);

  // Normal mapping: Transform tangent-space normals to world space
  if ((material.textureFlags & (1u << 1)) != 0u) // Normal map
  {
    // Sample tangent-space normal from texture (stored as [0,1], convert to [-1,1])
    vec3 tangentNormal = texture(globalTextures[nonuniformEXT(material.normalIndex)], uv).xyz * 2.0 - 1.0;

    // Flip Y for DirectX normal maps (OpenGL/Vulkan: Y-up, DirectX: Y-down)
    tangentNormal.y = -tangentNormal.y;

    // Construct TBN (Tangent-Bitangent-Normal) matrix for transformation to world space
    // Handle horizontal surfaces (floor/ceiling) differently to avoid tangent instability
    vec3 T;
    if (abs(N.y) > 0.99) // Surface is nearly horizontal (normal points up/down)
    {
      T = vec3(1.0, 0.0, 0.0); // Use world X-axis as tangent
    }
    else // Regular vertical or angled surface
    {
      T = normalize(cross(N, vec3(0.0, 1.0, 0.0))); // Derive tangent from normal and world up
    }
    vec3 B   = normalize(cross(N, T));
    T        = normalize(cross(B, N)); // Re-orthogonalize tangent
    mat3 TBN = mat3(T, B, N);

    // Transform tangent-space normal to world space
    N = normalize(TBN * tangentNormal);
  }

  vec3 V = normalize(ubo.cameraPosition.xyz - fragmentWorldPos);

  // Build tangent space for anisotropy
  vec3 T = normalize(cross(N, vec3(0.0, 1.0, 0.0)));
  if (length(T) < 0.01) T = normalize(cross(N, vec3(1.0, 0.0, 0.0)));
  vec3 B = cross(N, T);

  // Rotate tangent for anisotropic direction
  if (material.anisotropic > 0.01)
  {
    float angle = material.anisotropicRotation * 2.0 * PI;
    float cosA  = cos(angle);
    float sinA  = sin(angle);
    vec3  Trot  = cosA * T + sinA * B;
    vec3  Brot  = -sinA * T + cosA * B;
    T           = Trot;
    B           = Brot;
  }

  // Calculate reflectance at normal incidence
  // For dielectrics use 0.04, for metals use albedo
  vec3 F0 = vec3(0.04);

  // Apply IOR if provided (F0 = ((ior-1)/(ior+1))^2)
  if (material.ior != 1.5)
  {
    float ior = material.ior;
    float f   = (ior - 1.0) / (ior + 1.0);
    F0        = vec3(f * f);
  }

  // Apply Iridescence to F0
  if (material.iridescence > 0.0)
  {
    vec3 iridescenceColor = evalIridescence(max(dot(N, V), 0.1), material.iridescenceThickness, material.iridescenceIOR);
    F0                    = mix(F0, iridescenceColor, material.iridescence);
  }

  F0 = mix(F0, albedo, metallic);

  // Reflectance equation - Base layer
  vec3 Lo = vec3(0.0);

  // Point lights
  for (int i = 0; i < ubo.pointLightCount; i++)
  {
    // Calculate per-light radiance
    vec3  lightDir  = ubo.pointLights[i].position.xyz - fragmentWorldPos;
    float distance2 = dot(lightDir, lightDir);
    float intensity = ubo.pointLights[i].color.w;

    // Light Culling: Skip if contribution is negligible (approx < 1/255)
    // intensity / distance2 < 0.004  =>  intensity / 0.004 < distance2
    if (distance2 > intensity * 250.0) continue;

    float distance    = sqrt(distance2);
    vec3  L           = lightDir / distance; // normalize
    vec3  H           = normalize(V + L);
    float attenuation = 1.0 / distance2;
    vec3  radiance    = ubo.pointLights[i].color.xyz * intensity * attenuation;

    // Calculate shadow for point light using cube shadow map
    float shadow = 1.0;
    if (i < ubo.cubeShadowLightCount)
    {
      shadow = calculatePointLightShadow(fragmentWorldPos, i);
    }

    // Cook-Torrance BRDF with optional anisotropy
    float NDF;
    if (material.anisotropic > 0.01)
    {
      NDF = DistributionGGXAnisotropic(N, H, T, B, roughness, material.anisotropic);
    }
    else
    {
      NDF = DistributionGGX(N, H, roughness);
    }

    float G = GeometrySmith(N, V, L, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3  specular    = numerator / denominator;

    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
  }

  // Directional lights
  for (int i = 0; i < ubo.directionalLightCount; i++)
  {
    vec3 L        = normalize(-ubo.directionalLights[i].direction.xyz);
    vec3 H        = normalize(V + L);
    vec3 radiance = ubo.directionalLights[i].color.xyz * ubo.directionalLights[i].color.w;

    // Calculate shadow for first directional light
    float shadow = 1.0;
    if (i == 0 && ubo.shadowLightCount > 0)
    {
      shadow = calculateShadow(fragmentWorldPos, 0);
    }

    float NDF;
    if (material.anisotropic > 0.01)
    {
      NDF = DistributionGGXAnisotropic(N, H, T, B, roughness, material.anisotropic);
    }
    else
    {
      NDF = DistributionGGX(N, H, roughness);
    }

    float G = GeometrySmith(N, V, L, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3  specular    = numerator / denominator;

    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
  }

  // Spot lights
  for (int i = 0; i < ubo.spotLightCount; i++)
  {
    vec3  lightDir = ubo.spotLights[i].position.xyz - fragmentWorldPos;
    float distance = length(lightDir);
    vec3  L        = normalize(lightDir);
    vec3  H        = normalize(V + L);

    // Spotlight intensity calculation
    vec3  spotDir   = normalize(-ubo.spotLights[i].direction.xyz);
    float theta     = dot(L, spotDir);
    float epsilon   = ubo.spotLights[i].direction.w - ubo.spotLights[i].outerCutoff;
    float intensity = clamp((theta - ubo.spotLights[i].outerCutoff) / epsilon, 0.0, 1.0);

    // Attenuation
    float attenuation =
            1.0 / (ubo.spotLights[i].constantAtten + ubo.spotLights[i].linearAtten * distance + ubo.spotLights[i].quadraticAtten * distance * distance);

    vec3 radiance = ubo.spotLights[i].color.xyz * ubo.spotLights[i].color.w * attenuation * intensity;

    // Calculate shadow for this spotlight
    // Shadow map index = 1 + i (index 0 is for directional light)
    int   shadowIndex = 1 + i;
    float shadow      = 1.0;
    if (shadowIndex < ubo.shadowLightCount)
    {
      shadow = calculateShadow(fragmentWorldPos, shadowIndex);
    }

    float NDF;
    if (material.anisotropic > 0.01)
    {
      NDF = DistributionGGXAnisotropic(N, H, T, B, roughness, material.anisotropic);
    }
    else
    {
      NDF = DistributionGGX(N, H, roughness);
    }

    float G = GeometrySmith(N, V, L, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3  specular    = numerator / denominator;

    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
  }

  // Clearcoat layer (second specular lobe)
  if (material.clearcoat > 0.01)
  {
    vec3 clearcoatF0 = vec3(0.04); // Dielectric clearcoat
    vec3 clearcoatLo = vec3(0.0);

    // Point lights
    for (int i = 0; i < ubo.pointLightCount; i++)
    {
      vec3  lightDir    = ubo.pointLights[i].position.xyz - fragmentWorldPos;
      float distance    = length(lightDir);
      vec3  L           = normalize(lightDir);
      vec3  H           = normalize(V + L);
      float attenuation = 1.0 / (distance * distance);
      vec3  radiance    = ubo.pointLights[i].color.xyz * ubo.pointLights[i].color.w * attenuation;

      float clearNDF = DistributionGGX(N, H, material.clearcoatRoughness);
      float clearG   = GeometrySmith(N, V, L, material.clearcoatRoughness);
      vec3  clearF   = fresnelSchlick(max(dot(H, V), 0.0), clearcoatF0);

      vec3  numerator   = clearNDF * clearG * clearF;
      float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
      vec3  specular    = numerator / denominator;

      float NdotL = max(dot(N, L), 0.0);
      clearcoatLo += specular * radiance * NdotL;
    }

    // Directional lights
    for (int i = 0; i < ubo.directionalLightCount; i++)
    {
      vec3 L        = normalize(-ubo.directionalLights[i].direction.xyz);
      vec3 H        = normalize(V + L);
      vec3 radiance = ubo.directionalLights[i].color.xyz * ubo.directionalLights[i].color.w;

      float clearNDF = DistributionGGX(N, H, material.clearcoatRoughness);
      float clearG   = GeometrySmith(N, V, L, material.clearcoatRoughness);
      vec3  clearF   = fresnelSchlick(max(dot(H, V), 0.0), clearcoatF0);

      vec3  numerator   = clearNDF * clearG * clearF;
      float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
      vec3  specular    = numerator / denominator;

      float NdotL = max(dot(N, L), 0.0);
      clearcoatLo += specular * radiance * NdotL;
    }

    // Spot lights
    for (int i = 0; i < ubo.spotLightCount; i++)
    {
      vec3  lightDir = ubo.spotLights[i].position.xyz - fragmentWorldPos;
      float distance = length(lightDir);
      vec3  L        = normalize(lightDir);
      vec3  H        = normalize(V + L);

      vec3  spotDir   = normalize(-ubo.spotLights[i].direction.xyz);
      float theta     = dot(L, spotDir);
      float epsilon   = ubo.spotLights[i].direction.w - ubo.spotLights[i].outerCutoff;
      float intensity = clamp((theta - ubo.spotLights[i].outerCutoff) / epsilon, 0.0, 1.0);

      float attenuation =
              1.0 / (ubo.spotLights[i].constantAtten + ubo.spotLights[i].linearAtten * distance + ubo.spotLights[i].quadraticAtten * distance * distance);

      vec3 radiance = ubo.spotLights[i].color.xyz * ubo.spotLights[i].color.w * attenuation * intensity;

      float clearNDF = DistributionGGX(N, H, material.clearcoatRoughness);
      float clearG   = GeometrySmith(N, V, L, material.clearcoatRoughness);
      vec3  clearF   = fresnelSchlick(max(dot(H, V), 0.0), clearcoatF0);

      vec3  numerator   = clearNDF * clearG * clearF;
      float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
      vec3  specular    = numerator / denominator;

      float NdotL = max(dot(N, L), 0.0);
      clearcoatLo += specular * radiance * NdotL;
    }

    // Blend base layer with clearcoat
    Lo = mix(Lo, Lo + clearcoatLo * material.clearcoat, material.clearcoat);
  }

  // Ambient lighting (IBL)
  vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

  vec3 kS = F;
  vec3 kD = 1.0 - kS;
  kD *= 1.0 - metallic;

  // Flip Y for Vulkan cube map sampling
  // vec3 N_flip     = vec3(N.x, -N.y, N.z);
  vec3 N_flip     = N;
  vec3 irradiance = texture(irradianceMap, N_flip).rgb;
  vec3 diffuse    = irradiance * albedo;

  // Sample prefiltered map
  const float MAX_REFLECTION_LOD = 4.0;
  vec3        R                  = reflect(-V, N);
  // vec3        R_flip             = vec3(R.x, -R.y, R.z);
  vec3 R_flip           = R;
  vec3 prefilteredColor = textureLod(prefilterMap, R_flip, roughness * MAX_REFLECTION_LOD).rgb;

  // Sample BRDF LUT
  vec2 brdf     = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
  vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

  vec3 ambient = (kD * diffuse + specular) * ao;

  // Add simple ambient as fallback/boost
  ambient += ubo.ambientLightColor.xyz * ubo.ambientLightColor.w * albedo * ao * 0.05;

  // DEBUG: Disable ambient to see emissive clearly
  // ambient = vec3(0.0);

  // Emissive
  vec3 emissive = material.emissiveInfo.rgb * material.emissiveInfo.a;

  if ((material.textureFlags & (1u << 5)) != 0u) // Emissive texture
  {
    vec3 emissiveTex = texture(globalTextures[nonuniformEXT(material.emissiveIndex)], uv).rgb;
    emissive *= emissiveTex;
  }

  vec3 color = ambient + Lo + emissive;

  // HDR tonemapping (Reinhard)
  color = color / (color + vec3(1.0));
  // Gamma correction
  color = pow(color, vec3(1.0 / 2.2));

  // Add selection highlight (bright cyan outline effect)
  if (material.isSelected > 0.5)
  {
    // Create pulsing effect based on time (using position as pseudo-time)
    float pulse = 0.7 + 0.3 * sin(fragmentWorldPos.x + fragmentWorldPos.y + fragmentWorldPos.z);
    // Rim lighting effect for selection
    float rimIntensity  = 1.0 - abs(dot(N, V));
    rimIntensity        = pow(rimIntensity, 2.0);
    vec3 selectionColor = vec3(0.0, 1.0, 1.0) * pulse * 0.5; // Cyan highlight
    color += selectionColor * rimIntensity;
  }

  outColor = vec4(color, alpha * (1.0 - material.transmission));
}
