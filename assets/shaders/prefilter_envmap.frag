#version 450
#extension GL_GOOGLE_include_directive : enable

#include "includes/ibl_sampling.glsl"

layout(location = 0) in vec3 localPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform samplerCube environmentMap;

layout(push_constant) uniform PushConstants {
    mat4  viewProjection;
    int   faceIndex;
    float roughness;
    uint  sampleCount;
}
push;

void main() {
    vec3  N                = normalize(localPos);
    vec3  R                = N;
    vec3  V                = R;
    float totalWeight      = 0.0;
    vec3  prefilteredColor = vec3(0.0);

    for (uint i = 0u; i < push.sampleCount; ++i) {
        vec2  Xi    = Hammersley(i, push.sampleCount);
        vec3  H     = ImportanceSampleGGX(Xi, N, push.roughness);
        vec3  L     = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(dot(N, L), 0.0);

        if (NdotL > 0.0) {
            // Sample from the environment's mip level based on roughness/pdf
            float D     = DistributionGGX(N, H, push.roughness);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf   = D * NdotH / (4.0 * HdotV) + 0.0001;

            float resolution = 512.0; // Resolution of source cubemap (per face)
            float saTexel    = 4.0 * PI / (6.0 * resolution * resolution);
            float saSample   = 1.0 / (float(push.sampleCount) * pdf + 0.0001);
            float mipLevel   = push.roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

            // Flip Y for Vulkan cube map convention
            vec3 sampleDir = vec3(L.x, -L.y, L.z);

            prefilteredColor += textureLod(environmentMap, sampleDir, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;
    outColor         = vec4(prefilteredColor, 1.0);
}
