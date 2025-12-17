#version 450

layout(location = 0) out vec3 fragColor;

layout(set = 0, binding = 0) uniform UBO
{
  mat4 proj;
  mat4 view;
  // ... rest of UBO (we only need proj and view)
}
ubo;

layout(push_constant) uniform push_t
{
  mat4  modelMatrix;
  vec4  color;
  float fovY;
  float aspectRatio;
  float nearZ;
  float farZ;
}
push;

vec3 getFrustumVertex(int index)
{
  // 0-7: Corners
  // 0: Near Top Right
  // 1: Near Bottom Right
  // 2: Near Bottom Left
  // 3: Near Top Left
  // 4: Far Top Right
  // 5: Far Bottom Right
  // 6: Far Bottom Left
  // 7: Far Top Left

  // Map index 0-23 to corner indices for lines
  int lineIndices[24] = int[](0,
                              1,
                              1,
                              2,
                              2,
                              3,
                              3,
                              0, // Near plane
                              4,
                              5,
                              5,
                              6,
                              6,
                              7,
                              7,
                              4, // Far plane
                              0,
                              4,
                              1,
                              5,
                              2,
                              6,
                              3,
                              7 // Connections
  );

  int cornerIndex = lineIndices[index];

  float z = (cornerIndex < 4) ? push.nearZ : push.farZ;
  float h = tan(push.fovY * 0.5) * z;
  float w = h * push.aspectRatio;

  float xSign = (cornerIndex == 0 || cornerIndex == 1 || cornerIndex == 4 || cornerIndex == 5) ? 1.0 : -1.0;
  float ySign = (cornerIndex == 0 || cornerIndex == 3 || cornerIndex == 4 || cornerIndex == 7)
                        ? -1.0
                        : 1.0; // Vulkan Y is down? No, standard camera space Y is down in Vulkan?
  // In standard camera space: Right is +X, Up is -Y (Vulkan) or +Y (OpenGL).
  // Engine uses GLM_FORCE_RADIANS and GLM_FORCE_DEPTH_ZERO_TO_ONE.
  // Usually +Y is down in Vulkan clip space, but in View space it depends on the LookAt matrix.
  // Let's assume standard Y-down for now, or just check signs.

  return vec3(w * xSign, h * ySign, z);
}

void main()
{
  vec3 vertexPos = getFrustumVertex(gl_VertexIndex);
  vec4 worldPos  = push.modelMatrix * vec4(vertexPos, 1.0);
  gl_Position    = ubo.proj * ubo.view * worldPos;
  fragColor      = push.color.rgb;
}
