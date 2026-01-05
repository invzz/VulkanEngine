// Shared scene UBO used by the main render passes (gbuffer, deferred lighting, forward compositor).
// Intentionally contains no #version or extensions; those stay in the including shader.

struct PointLight {
    vec4 position;
    vec4 color;
    float radius2;
    float _pad0;
    float _pad1;
    float _pad2;
};

struct DirectionalLight {
    vec4 direction;
    vec4 color;
};

struct SpotLight {
    vec4  position;
    vec4  direction;
    vec4  color;
    float outerCutoff;
    float constantAtten;
    float linearAtten;
    float quadraticAtten;
    float radius2;
    float _pad0;
    float _pad1;
    float _pad2;
};

layout(set = 0, binding = 0) uniform UBO {
    mat4             proj;
    mat4             view;
    vec4             ambientLightColor;
    vec4             cameraPosition;
    PointLight       pointLights[16];
    DirectionalLight directionalLights[16];
    SpotLight        spotLights[16];
    mat4             lightSpaceMatrices[16];
    vec4             pointLightShadowData[4];
    int              pointLightCount;
    int              directionalLightCount;
    int              spotLightCount;
    int              shadowLightCount;
    int              cubeShadowLightCount;
    int              debugMode;
    int              _pad2;
    int              _pad3;
    vec4             frustumPlanes[6];
    vec4             fogColor;
    vec4             fogZenithColor;
    float            fogHeight;
    float            fogHeightDensity;
    float            _pad4;
    float            _pad5;
} ubo;
