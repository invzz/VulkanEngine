// Shared material decoding helpers used by gbuffer + forward compositor.
// Intentionally contains no #version or extensions; those stay in the including shader.

// Bindless textures (set 1)
layout(set = 1, binding = 0) uniform sampler2D globalTextures[];

// Material parameters (set 4)
layout(set = 4, binding = 0) uniform MaterialData {
    vec4  albedo;
    vec4  emissiveInfo;             // rgb: color, a: strength
    vec4  specularGlossinessFactor; // legacy/unused in compositor
    vec4  attenuationColorAndDist;  // rgb: attenuation color, a: attenuation distance
    mat4  params;                   // packed float parameters
    uvec4 flagsAndIndices0;
    uvec4 indices1;
    uvec4 indices2;
    uvec4 indices3;
} material;

// Material conventions (current engine packing):
// - params[3][1] : UV scale
// - params[3][2] : alpha cutoff (MASK)
// - params[2][1] : IOR
// - params[2][0] : transmission factor
// - params[0][0] : metallic factor
// - params[0][1] : roughness factor
// - params[0][2] : AO factor
// - indices1: x=metallicTex, y=roughnessTex or packed MR/ORM tex, z=aoTex, w=emissiveTex
// - flagsAndIndices0: x=flags bitfield, y=alphaMode, z=baseColorTex, w=normalTex

struct AlphaOnly {
    vec3  albedo;
    float alpha;
};

vec2 material_getUv(vec2 baseUv) {
    return baseUv * material.params[3][1];
}

AlphaOnly material_computeAlphaOnly(vec2 uv) {
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

    if (isAlphaMasked) {
        if (alpha < material.params[3][2]) {
            discard;
        }
        alpha = 1.0;
    } else if (isOpaque) {
        alpha = 1.0;
    }

    AlphaOnly result;
    result.albedo = albedo;
    result.alpha  = alpha;
    return result;
}

void material_decodeMetallicRoughness(vec2 uv, inout float metallic, inout float roughness) {
    uint materialFlags = material.flagsAndIndices0.x;
    bool hasMetallicTexture               = (materialFlags & (1u << 2)) != 0u;
    bool hasRoughnessTexture              = (materialFlags & (1u << 3)) != 0u;
    bool metallicRoughnessPacked          = (materialFlags & (1u << 6)) != 0u;
    bool occlusionRoughnessMetallicPacked = (materialFlags & (1u << 7)) != 0u;

    if (occlusionRoughnessMetallicPacked) {
        vec4 ormSample = texture(globalTextures[nonuniformEXT(material.indices1.y)], uv);
        roughness      = ormSample.g;
        metallic       = ormSample.b;
    } else if (metallicRoughnessPacked) {
        vec4 mrSample = texture(globalTextures[nonuniformEXT(material.indices1.y)], uv);
        metallic *= mrSample.b;
        roughness *= mrSample.g;
    } else {
        float metallicMask  = hasMetallicTexture ? 1.0 : 0.0;
        float roughnessMask = hasRoughnessTexture ? 1.0 : 0.0;

        uint metallicIndex  = hasMetallicTexture ? material.indices1.x : 0u;
        uint roughnessIndex = hasRoughnessTexture ? material.indices1.y : 0u;

        float metallicTex  = texture(globalTextures[nonuniformEXT(metallicIndex)], uv).r;
        float roughnessTex = texture(globalTextures[nonuniformEXT(roughnessIndex)], uv).r;

        metallic *= mix(1.0, metallicTex, metallicMask);
        roughness *= mix(1.0, roughnessTex, roughnessMask);
    }
}

void material_decodeMetallicRoughnessAo(vec2 uv, inout float metallic, inout float roughness, inout float ao) {
    uint materialFlags = material.flagsAndIndices0.x;
    bool hasAoTexture                     = (materialFlags & (1u << 4)) != 0u;
    bool metallicRoughnessPacked          = (materialFlags & (1u << 6)) != 0u;
    bool occlusionRoughnessMetallicPacked = (materialFlags & (1u << 7)) != 0u;

    bool aoHandled = false;

    if (occlusionRoughnessMetallicPacked) {
        vec4 ormSample = texture(globalTextures[nonuniformEXT(material.indices1.y)], uv);
        ao             = ormSample.r;
        roughness      = ormSample.g;
        metallic       = ormSample.b;
        aoHandled      = true;
    } else if (metallicRoughnessPacked) {
        vec4 mrSample = texture(globalTextures[nonuniformEXT(material.indices1.y)], uv);
        metallic *= mrSample.b;
        roughness *= mrSample.g;
    } else {
        material_decodeMetallicRoughness(uv, metallic, roughness);
    }

    if (!aoHandled) {
        float aoMask  = hasAoTexture ? 1.0 : 0.0;
        uint  aoIndex = hasAoTexture ? material.indices1.z : 0u;
        float aoTex   = texture(globalTextures[nonuniformEXT(aoIndex)], uv).r;
        ao *= mix(1.0, aoTex, aoMask);
    }
}

vec3 material_decodeNormalWorld(vec2 uv, vec3 baseNormalWorld) {
    uint materialFlags = material.flagsAndIndices0.x;
    bool hasNormalMap  = (materialFlags & (1u << 1)) != 0u;

    vec3 N = normalize(baseNormalWorld);
    if (hasNormalMap) {
        vec3 tangentNormal = texture(globalTextures[nonuniformEXT(material.flagsAndIndices0.w)], uv).xyz * 2.0 - 1.0;
        tangentNormal.y    = -tangentNormal.y;

        vec3 T, B;
        buildOrthonormalBasis(N, T, B);
        mat3 TBN = mat3(T, B, N);
        N = normalize(TBN * tangentNormal);
    }
    return N;
}

vec3 material_decodeEmissive(vec2 uv) {
    vec3 emissive = material.emissiveInfo.rgb * material.emissiveInfo.a;

    uint materialFlags      = material.flagsAndIndices0.x;
    bool hasEmissiveTexture = (materialFlags & (1u << 5)) != 0u;
    if (hasEmissiveTexture) {
        emissive *= texture(globalTextures[nonuniformEXT(material.indices1.w)], uv).rgb;
    }
    return emissive;
}

float material_decodeTransmission(vec2 uv) {
    uint materialFlags = material.flagsAndIndices0.x;
    bool hasTransmissionTexture = (materialFlags & (1u << 9)) != 0u;

    float transmission = material.params[2][0];
    if (hasTransmissionTexture) {
        transmission *= texture(globalTextures[nonuniformEXT(material.indices2.z)], uv).r;
    }
    return transmission;
}

vec3 material_computeF0(vec3 albedo, float metallic) {
    vec3 F0 = vec3(0.04);

    float ior = material.params[2][1];
    if (abs(ior - 1.5) > 1e-4) {
        float f = (ior - 1.0) / (ior + 1.0);
        F0 = vec3(f * f);
    }

    return mix(F0, albedo, metallic);
}
