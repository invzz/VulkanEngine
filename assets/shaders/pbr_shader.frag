#version 450

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

layout(set = 0, binding = 0) uniform UBO
{
  mat4       proj;
  mat4       view;
  vec4       ambientLightColor;
  vec4       cameraPosition;
  PointLight pointLights[16];
  mat4       lightSpaceMatrices[16]; // Light space transformation matrices for shadows
  int        numberOfLights;
  int        shadowLightCount; // Number of lights casting shadows
}
ubo;

// Material textures (set 1)
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicMap;
layout(set = 1, binding = 3) uniform sampler2D roughnessMap;
layout(set = 1, binding = 4) uniform sampler2D aoMap;

// Push constants: Only material portion visible in fragment shader (offset 128)
layout(push_constant) uniform PushConstants
{
  layout(offset = 128) vec3 albedo;
  float                     metallic;
  float                     roughness;
  float                     ao;
  float                     isSelected;          // 1.0 if selected, 0.0 otherwise
  float                     clearcoat;           // Clearcoat layer strength
  float                     clearcoatRoughness;  // Clearcoat roughness
  float                     anisotropic;         // Anisotropy strength
  float                     anisotropicRotation; // Anisotropic rotation
  uint                      textureFlags;        // Bit flags: bit 0=albedo, 1=normal, 2=metallic, 3=roughness, 4=ao
  float                     uvScale;             // UV tiling scale
}
push;

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

// Shadow system removed - to be reimplemented later

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

void main()
{
  // Apply UV tiling
  vec2 uv = fragUV * push.uvScale;

  // Sample material properties from textures or use push constants
  vec3  albedo    = push.albedo;
  float metallic  = push.metallic;
  float roughness = push.roughness;
  float ao        = push.ao;

  // Check texture flags and sample if present
  if ((push.textureFlags & (1u << 0)) != 0u) // Albedo map
  {
    vec4 texColor = texture(albedoMap, uv);
    albedo        = texColor.rgb; // Already linear (loaded as sRGB format, auto-converted)
  }

  if ((push.textureFlags & (1u << 2)) != 0u) // Metallic map
  {
    metallic = texture(metallicMap, uv).r;
  }

  if ((push.textureFlags & (1u << 3)) != 0u) // Roughness map
  {
    roughness = texture(roughnessMap, uv).r;
  }

  if ((push.textureFlags & (1u << 4)) != 0u) // AO map
  {
    ao = texture(aoMap, uv).r;
  }

  vec3 N = normalize(fragmentNormalWorld);

  // Apply normal map if present
  if ((push.textureFlags & (1u << 1)) != 0u) // Normal map
  {
    // Sample tangent-space normal
    vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;

    // Flip Y for DirectX normal maps (OpenGL/Vulkan has Y pointing up, DirectX has Y pointing down)
    tangentNormal.y = -tangentNormal.y;

    // Build TBN matrix - for horizontal surfaces, use Z as up reference
    vec3 T;
    if (abs(N.y) > 0.99) // Surface is horizontal (floor/ceiling)
    {
      T = vec3(1.0, 0.0, 0.0); // Use X-axis as tangent
    }
    else // Regular vertical or angled surface
    {
      T = normalize(cross(N, vec3(0.0, 1.0, 0.0)));
    }
    vec3 B   = normalize(cross(N, T));
    T        = normalize(cross(B, N)); // Re-orthogonalize
    mat3 TBN = mat3(T, B, N);

    // Transform to world space
    N = normalize(TBN * tangentNormal);
  }

  vec3 V = normalize(ubo.cameraPosition.xyz - fragmentWorldPos);

  // Build tangent space for anisotropy
  vec3 T = normalize(cross(N, vec3(0.0, 1.0, 0.0)));
  if (length(T) < 0.01) T = normalize(cross(N, vec3(1.0, 0.0, 0.0)));
  vec3 B = cross(N, T);

  // Rotate tangent for anisotropic direction
  if (push.anisotropic > 0.01)
  {
    float angle = push.anisotropicRotation * 2.0 * PI;
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
  F0      = mix(F0, albedo, metallic);

  // Reflectance equation - Base layer
  vec3 Lo = vec3(0.0);
  for (int i = 0; i < ubo.numberOfLights; i++)
  {
    // Calculate per-light radiance
    vec3  lightDir    = ubo.pointLights[i].position.xyz - fragmentWorldPos;
    float distance    = length(lightDir);
    vec3  L           = normalize(lightDir);
    vec3  H           = normalize(V + L);
    float attenuation = 1.0 / (distance * distance);
    vec3  radiance    = ubo.pointLights[i].color.xyz * ubo.pointLights[i].color.w * attenuation;

    // No shadows for now
    float shadow = 1.0;

    // Cook-Torrance BRDF with optional anisotropy
    float NDF;
    if (push.anisotropic > 0.01)
    {
      NDF = DistributionGGXAnisotropic(N, H, T, B, roughness, push.anisotropic);
    }
    else
    {
      NDF = DistributionGGX(N, H, roughness);
    }

    float G = GeometrySmith(N, V, L, roughness);
    vec3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; // Metallic surfaces don't have diffuse

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3  specular    = numerator / denominator;

    // Add to outgoing radiance Lo (apply shadow)
    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo / PI + specular) * radiance * NdotL * shadow;
  }

  // Clearcoat layer (second specular lobe)
  if (push.clearcoat > 0.01)
  {
    vec3 clearcoatF0 = vec3(0.04); // Dielectric clearcoat
    vec3 clearcoatLo = vec3(0.0);

    for (int i = 0; i < ubo.numberOfLights; i++)
    {
      vec3  lightDir    = ubo.pointLights[i].position.xyz - fragmentWorldPos;
      float distance    = length(lightDir);
      vec3  L           = normalize(lightDir);
      vec3  H           = normalize(V + L);
      float attenuation = 1.0 / (distance * distance);
      vec3  radiance    = ubo.pointLights[i].color.xyz * ubo.pointLights[i].color.w * attenuation;

      float clearNDF = DistributionGGX(N, H, push.clearcoatRoughness);
      float clearG   = GeometrySmith(N, V, L, push.clearcoatRoughness);
      vec3  clearF   = fresnelSchlick(max(dot(H, V), 0.0), clearcoatF0);

      vec3  numerator   = clearNDF * clearG * clearF;
      float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
      vec3  specular    = numerator / denominator;

      float NdotL = max(dot(N, L), 0.0);
      clearcoatLo += specular * radiance * NdotL;
    }

    // Blend base layer with clearcoat
    Lo = mix(Lo, Lo + clearcoatLo * push.clearcoat, push.clearcoat);
  }

  // Ambient lighting (simple approximation)
  vec3 ambient = ubo.ambientLightColor.xyz * ubo.ambientLightColor.w * albedo * ao;
  vec3 color   = ambient + Lo;

  // HDR tonemapping (Reinhard)
  color = color / (color + vec3(1.0));
  // Gamma correction
  color = pow(color, vec3(1.0 / 2.2));

  // Add selection highlight (bright cyan outline effect)
  if (push.isSelected > 0.5)
  {
    // Create pulsing effect based on time (using position as pseudo-time)
    float pulse = 0.7 + 0.3 * sin(fragmentWorldPos.x + fragmentWorldPos.y + fragmentWorldPos.z);
    // Rim lighting effect for selection
    float rimIntensity  = 1.0 - abs(dot(N, V));
    rimIntensity        = pow(rimIntensity, 2.0);
    vec3 selectionColor = vec3(0.0, 1.0, 1.0) * pulse * 0.5; // Cyan highlight
    color += selectionColor * rimIntensity;
  }

  outColor = vec4(color, 1.0);
}
