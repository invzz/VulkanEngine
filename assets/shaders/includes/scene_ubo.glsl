// Shared scene UBO used by the main render passes (gbuffer, deferred lighting, forward compositor).
// Intentionally contains no #version or extensions; those stay in the including shader.

struct PointLight
{
  vec4  position;
  vec4  color;
  float radius2;
  float _pad0;
  float _pad1;
  float _pad2;
};

struct DirectionalLight
{
  vec4 direction;
  vec4 color;
};

struct SpotLight
{
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

layout(set = 0, binding = 0, std140) uniform UBO
{
  mat4  proj;
  mat4  view;
  vec4  ambientLightColor;
  vec4  cameraPosition;
  mat4  lightSpaceMatrices[16];
  vec4  pointLightShadowData[4];
  vec4  directionalCascadeSplits;
  float cascadeBlendWidth;
  int   pointLightCount;
  int   directionalLightCount;
  int   spotLightCount;
  int   shadowLightCount;
  int   cubeShadowLightCount;
  int   directionalCascadeCount;
  int   directionalCascadeBaseIndex;
  int   debugMode;
  int   _reservedDebug; // Previously bakedDebugRaw, kept for layout compatibility
  // Note: float + 9 ints = 40 bytes; std140 will insert padding so the following vec4 array starts on a 16-byte boundary
  vec4  frustumPlanes[6];
  vec4  fogColor;
  vec4  fogZenithColor;
  float fogHeight;
  float fogHeightDensity;
  // Padding to align HZB settings to 16-byte boundary
  float _padFog0;
  float _padFog1;
  // HZB occlusion culling settings - now at 16-byte aligned offset
  int   hzbMaxMipLevel;     // Maximum mip level for HZB testing
  float hzbMinScreenPixels; // Skip HZB for objects smaller than this in pixels
  float hzbScreenSizeScale; // Mip selection bias (higher = coarser mips)
  int   hzbEnabled;         // 0 = disabled, 1 = enabled
}
ubo;

layout(set = 0, binding = 3, std430) readonly buffer PointLightBuffer
{
  PointLight pointLights[];
};

layout(set = 0, binding = 4, std430) readonly buffer DirectionalLightBuffer
{
  DirectionalLight directionalLights[];
};

layout(set = 0, binding = 5, std430) readonly buffer SpotLightBuffer
{
  SpotLight spotLights[];
};
