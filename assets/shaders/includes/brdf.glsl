#ifndef BRDF_GLSL
#define BRDF_GLSL

// Common BRDF and microfacet helpers extracted for reuse
const float PI = 3.14159265359;

// GGX/Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Smith's Geometry Shadowing Function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel-Schlick Approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Burley (Disney) diffuse — adds roughness-dependent retro-reflection
// for more natural diffuse on rough/matte surfaces
float burleyDiffuse(float NdotL, float NdotV, float NdotH, float roughness) {
    float a      = roughness * roughness;
    float fd90   = 0.5 + 2.0 * a * NdotH * NdotH;
    float fLam1  = 1.0 + (fd90 - 1.0) * pow(1.0 - NdotL, 5.0);
    float fLam2  = 1.0 + (fd90 - 1.0) * pow(1.0 - NdotV, 5.0);
    return fLam1 * fLam2;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
    pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Anisotropic GGX Distribution
float DistributionGGXAnisotropic(vec3 N, vec3 H, vec3 T, vec3 B,
    float roughness, float anisotropy) {
    float alpha = roughness * roughness;
    float at = max(alpha * (1.0 + anisotropy), 0.001);
    float ab = max(alpha * (1.0 - anisotropy), 0.001);

    float ToH = dot(T, H);
    float BoH = dot(B, H);
    float NoH = dot(N, H);

    float a2 = at * ab;
    vec3 v = vec3(ab * ToH, at * BoH, a2 * NoH);
    float v2 = dot(v, v);
    float w2 = a2 / v2;

    return a2 * w2 * w2 / PI;
}

// Specular Occlusion (Lagarde/Filament)
float computeSpecularAO(float NdotV, float ao, float roughness) {
    return clamp(pow(NdotV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0,
        1.0);
}

#endif // BRDF_GLSL
