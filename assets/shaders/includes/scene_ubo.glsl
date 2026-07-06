// Shared scene UBO used by the main render passes (gbuffer, deferred lighting, forward compositor).
// Intentionally contains no #version or extensions; those stay in the including shader.

struct PointLight {
    vec4 positionRadius2;  // xyz = position, w = radius^2
    vec4 colorIntensity;   // rgb = color, w = intensity
};

struct DirectionalLight {
    vec4 direction;
    vec4 color;
};

struct SpotLight {
    vec4 positionRadius2;  // xyz = position, w = radius^2
    vec4 directionInner;   // xyz = direction, w = inner cutoff (cos)
    vec4 colorIntensity;   // rgb = color, w = intensity
    vec4 attenOuter;       // x = outer cutoff (cos), y = constant, z = linear, w = quadratic
};

layout(set = 0, binding = 0, std140) uniform UBO {
    mat4 proj;
    mat4 view;
    mat4 invProj;  // CPU-provided inverse projection (avoid GPU inverse())
    mat4 invView;  // CPU-provided inverse view (avoid GPU inverse())
    vec4 ambientLightColor;
    vec4 cameraPosition;
    mat4 lightSpaceMatrices[8];
    mat4 cascadeLightMatrices[4];
    vec4 cascadeSplits;
    int  cascadeCount;
    int  _padCascade0;
    int  _padCascade1;
    int  _padCascade2;
    vec4 pointLightShadowData[4];
    int  pointLightCount;
    int  directionalLightCount;
    int  spotLightCount;
    int  shadowLightCount;
    int  cubeShadowLightCount;
    int  debugMode;
    int  rtDirectional;
    int  rtPoint;
    int  rtSpot;
    int  rtPad;
    int  rtPad2;
    int  _padDebug0;
    vec4 frustumPlanes[6];
}
ubo;

layout(set = 0, binding = 6, std140) uniform UBOCold {
    vec4 reservedCold;
}
uboCold;

layout(set = 0, binding = 3, std430) readonly buffer PointLightBuffer {
    PointLight pointLights[];
};

layout(set = 0, binding = 4, std430) readonly buffer DirectionalLightBuffer {
    DirectionalLight directionalLights[];
};

layout(set = 0, binding = 5, std430) readonly buffer SpotLightBuffer {
    SpotLight spotLights[];
};
