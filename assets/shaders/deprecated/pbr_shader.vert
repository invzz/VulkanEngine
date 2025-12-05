#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragmentWorldPos;
layout(location = 2) out vec3 fragmentNormalWorld;
layout(location = 3) out vec2 fragUV;

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
  int        numberOfLights;
}
ubo;

struct MeshBuffers
{
  uint64_t vertexBufferAddress;
  uint64_t indexBufferAddress;
};

layout(set = 0, binding = 1) readonly buffer MeshBuffer
{
  MeshBuffers meshes[];
}
meshBuffer;

struct Vertex
{
  vec3 position;
  vec3 color;
  vec3 normal;
  vec2 uv;
  int  materialId;
};

layout(buffer_reference, scalar) buffer VertexBuffer
{
  Vertex vertices[];
};

layout(push_constant) uniform PushConstants
{
  mat4  modelMatrix;
  mat4  normalMatrix;
  vec3  albedo;
  float metallic;
  float roughness;
  float ao;
  float isSelected;
  float clearcoat;
  float clearcoatRoughness;
  float anisotropic;
  float anisotropicRotation;
  uint  textureFlags;
  float uvScale;
  uint  albedoIndex;
  uint  normalIndex;
  uint  metallicIndex;
  uint  roughnessIndex;
  uint  aoIndex;
  uint  meshId;
}
push;

void main()
{
  vec3 pos;
  vec3 col;
  vec3 norm;
  vec2 tex;

  if (push.meshId != 0)
  {
    uint64_t     address = meshBuffer.meshes[push.meshId].vertexBufferAddress;
    VertexBuffer v       = VertexBuffer(address);
    Vertex       vertex  = v.vertices[gl_VertexIndex];
    pos                  = vertex.position;
    col                  = vertex.color;
    norm                 = vertex.normal;
    tex                  = vertex.uv;
  }
  else
  {
    pos  = inPosition;
    col  = inColor;
    norm = inNormal;
    tex  = inUV;
  }

  vec4 positionWorld = push.modelMatrix * vec4(pos, 1.0);
  gl_Position        = ubo.proj * ubo.view * positionWorld;

  vec3 normalWorldSpace = normalize(mat3(push.normalMatrix) * norm);

  fragColor           = col;
  fragmentWorldPos    = vec3(positionWorld);
  fragmentNormalWorld = normalWorldSpace;
  fragUV              = tex;
}
