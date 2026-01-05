#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : enable

#include "includes/common.glsl"
#include "includes/scene_ubo.glsl"
#include "includes/material_decode.glsl"

layout(location = 0) out vec4 outNormal;
layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec4 outMaterial;
layout(location = 3) out vec4 outEmissive;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragmentWorldPos;
layout(location = 2) in vec3 fragmentNormalWorld;
layout(location = 3) in vec2 fragUV;
layout(location = 4) in flat uint inMeshletId;
layout(location = 5) in flat vec3 inConeAxis;

void main() {
    vec2 uv = material_getUv(fragUV);
    AlphaOnly alphaOnly = material_computeAlphaOnly(uv);

    float metallic  = material.params[0][0];
    float roughness = material.params[0][1];
    float ao        = material.params[0][2];

    material_decodeMetallicRoughnessAo(uv, metallic, roughness, ao);
    vec3 N = material_decodeNormalWorld(uv, fragmentNormalWorld);
    vec3 emissive = material_decodeEmissive(uv);

    // Iridescence (packed into spare alpha channels for deferred shading)
    float iridescenceFactor    = material.params[2][2];
    float iridescenceIOR       = material.params[2][3];
    float iridescenceThickness = material.params[3][0];

    outNormal   = vec4(N, iridescenceIOR);
    outAlbedo   = vec4(alphaOnly.albedo, iridescenceThickness);
    outMaterial = vec4(metallic, roughness, ao, iridescenceFactor);
    outEmissive = vec4(emissive, 1.0);
}
