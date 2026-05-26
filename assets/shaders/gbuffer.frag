#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable

#include "includes/common.glsl"
#include "includes/material_decode.glsl"
#include "includes/mesh_push_constants.glsl"
#include "includes/scene_ubo.glsl"

layout(location = 0) out vec4 outNormal;
layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec4 outMaterial;
layout(location = 3) out vec4 outEmissive;
// location 4 reserved for compatibility (baked attachment removed)

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragmentWorldPos;
layout(location = 2) in vec3 fragmentNormalWorld;
layout(location = 3) in vec2 fragUV;
layout(location = 4) in flat uint inMeshletId;
layout(location = 5) in flat vec3 inConeAxis;

vec2 octEncode(vec3 n)
{
  n /= (abs(n.x) + abs(n.y) + abs(n.z) + 1e-6);
  vec2 p = n.xy;
  if (n.z < 0.0)
  {
    p = (1.0 - abs(p.yx)) * sign(p);
  }
  return p;
}

void main()
{
  vec2      uv        = material_getUv(fragUV);
  AlphaOnly alphaOnly = material_computeAlphaOnly(uv);

  float metallic  = material.params[0][0];
  float roughness = material.params[0][1];
  float ao        = material.params[0][2];

  material_decodeMetallicRoughnessAo(uv, metallic, roughness, ao);
  vec3 N        = material_decodeNormalWorld(uv, fragmentNormalWorld);
  N             = normalize(N);
  vec3 emissive = material_decodeEmissive(uv);

  // Debug views: Meshlets (7) and 8 (Meshlet Cones)
  if (ubo.debugMode == 7)
  {
    // Hash meshlet ID to a unique color
    uint h            = inMeshletId;
    h                 = ((h >> 16) ^ h) * 0x45d9f3b;
    h                 = ((h >> 16) ^ h) * 0x45d9f3b;
    h                 = (h >> 16) ^ h;
    vec3 meshletColor = vec3(float((h >> 0) & 0xFF) / 255.0, float((h >> 8) & 0xFF) / 255.0, float((h >> 16) & 0xFF) / 255.0);
    // Use emissive to pass debug color, set albedo alpha to signal debug mode
    outNormal   = vec4(0.5, 0.5, 1.0, 0.0);
    outAlbedo   = vec4(meshletColor, -1.0); // Negative alpha = debug flag
    outMaterial = vec4(0.0);
    outEmissive = vec4(meshletColor, 1.0);
    return;
  }
  if (ubo.debugMode == 8)
  {
    // Visualize cone axis as color
    vec3 coneColor = normalize(inConeAxis) * 0.5 + 0.5;
    outNormal      = vec4(0.5, 0.5, 1.0, 0.0);
    outAlbedo      = vec4(coneColor, -1.0); // Negative alpha = debug flag
    outMaterial    = vec4(0.0);
    outEmissive    = vec4(coneColor, 1.0);
    return;
  }

  // Material IOR (KHR_materials_ior)
  float materialIOR = material.params[2][1];

  // Iridescence (packed into spare alpha channels for deferred shading)
  float iridescenceFactor    = material.params[2][2];
  float iridescenceIOR       = material.params[2][3];
  float iridescenceThickness = material.params[3][0];

  // G-buffer normal packing:
  // - RG: oct-encoded world normal in [0,1]
  // - B : material IOR
  // - A : iridescence (thin film) IOR
  vec2 nOct = octEncode(N);

  outNormal   = vec4(nOct * 0.5 + 0.5, materialIOR, iridescenceIOR);

  // Option A: Albedo → RGBA8UNORM (4 bytes/pixel)
  // RGB quantized to 8-bit UNORM; A = iridescence thickness (may clamp > 1.0)
  outAlbedo = vec4(clamp(alphaOnly.albedo, 0.0, 1.0) * 255.0, iridescenceThickness) / 255.0;

  // Option B: Material → RG8UNORM (2 bytes/pixel)
  // R: metallic (high 8 bits) | roughness (low 8 bits)
  // G: AO
  uint mrPacked = uint(metallic * 255.0) << 8 | uint(roughness * 255.0);
  outMaterial   = vec4(vec2(uintBitsToFloat(mrPacked)), ao, iridescenceFactor);
  outEmissive = vec4(emissive, 1.0);
}
