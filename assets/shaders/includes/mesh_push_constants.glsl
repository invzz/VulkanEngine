// Shared push constant block for mesh pipelines. Must match C++ MeshPushConstantData layout exactly.
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(push_constant) uniform Push
{
  mat4 modelMatrix;
  mat4 normalMatrix;
  uint meshId;

  // 64-bit addresses used by task/mesh shaders
  uint64_t meshletBufferAddress;
  uint64_t meshletVerticesAddress;
  uint64_t meshletTrianglesAddress;
  uint64_t vertexBufferAddress;

  uint meshletOffset;
  uint meshletCount;
  vec2 screenSize;
  uint cullingFlags;
}
push;